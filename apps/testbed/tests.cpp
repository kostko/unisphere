/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <k@jst.sm>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "testbed/test_bed.h"
#include "testbed/dataset/graphs.hpp"
#include "social/compact_router.h"
#include "social/routing_table.h"
#include "social/name_database.h"
#include "social/sloppy_group.h"
#include "social/rpc_channel.h"
#include "rpc/engine.hpp"

#include "src/social/core_methods.pb.h"

#include <atomic>
#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>

using namespace UniSphere;

namespace Tests {

#if 0
class AllPairs : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /// Number of nodes at test start
  unsigned long numNodes;
  /// Number of expected responses
  unsigned long expected;
  /// Number of received responses
  std::atomic<unsigned long> received;
  /// Number of failures
  std::atomic<unsigned long> failures;
  /// Measured hop counts
  std::unordered_map<std::tuple<NodeIdentifier, NodeIdentifier>, int> pathLengths;
  /// Mutex
  std::mutex mutex;

  /**
   * Test if routing works for all pairs of nodes.
   */
  void start()
  {
    // Determine the number of nodes at test start
    numNodes = nodes().size();
    // Determine the number of expected responses
    expected = numNodes * numNodes;
    // Initialize the number of received responses
    received = 0;
    // Initialize the number of failures
    failures = 0;

    for (TestBed::VirtualNodePtr a : nodes() | boost::adaptors::map_values) {
      for (TestBed::VirtualNodePtr b : nodes() | boost::adaptors::map_values) {
        RpcEngine<SocialRpcChannel> &rpc = a->router->rpcEngine();

        // Transmit a ping request to each node and wait for a response
        Protocol::PingRequest request;
        request.set_timestamp(1);
        rpc.call<Protocol::PingRequest, Protocol::PingResponse>(b->nodeId, "Core.Ping", request,
          [this, a, b](const Protocol::PingResponse &rsp, const RoutedMessage &msg) {
            // Measure hop count difference to see the length of traversed path
            int pathLength = rsp.hopcount() - msg.hopCount();
            {
              UniqueLock lock(mutex);
              pathLengths[std::make_pair(a->nodeId, b->nodeId)] = pathLength;
            }

            received++;
            checkDone();
          },
          [this, a, b](RpcErrorCode, const std::string &msg) {
            failures++;
            BOOST_LOG_SEV(logger(), log::error) << "Pair = (" << a->name << ", " << b->name << ") RPC call failure: " << msg;
            checkDone();
          },
          rpc.options().setTimeout(45)
        );
      }
    }
  }

  /**
   * Checks if the test has been completed.
   */
  bool checkDone()
  {
    if ((received + failures) == expected)
      evaluate();
  }

  /**
   * Evaluate test results.
   */
  void evaluate()
  {
    // Test summary
    BOOST_LOG(logger()) << "All nodes = " << numNodes;
    BOOST_LOG(logger()) << "Received responses = " << received;
    BOOST_LOG(logger()) << "Failures = " << failures;

    // Requirements for passing the test
    require(received == expected);

    testbed.snapshot([this]() {
      // Prepare the data collector
      auto stretch = data("stretch", {"node_a", "node_b", "shortest", "measured", "stretch"});

      // Prepare global topology
      CompactRoutingTable::TopologyDumpGraph graph;
      for (TestBed::VirtualNodePtr node : nodes() | boost::adaptors::map_values) {
        node->router->routingTable().dumpTopology(graph);
      }

      // Compute all-pairs shortest paths
      typedef typename boost::graph_traits<CompactRoutingTable::TopologyDumpGraph>::vertex_descriptor vertex_des;
      std::unordered_map<vertex_des, std::unordered_map<vertex_des, int>> shortestLengths;
      boost::floyd_warshall_all_pairs_shortest_paths(graph.graph(), shortestLengths,
        boost::weight_map(boost::get(CompactRoutingTable::TopologyDumpTags::LinkWeight(), graph.graph())));

      // Compute routing stretch for each pair
      int n = 0;
      double averageStretch = 0.0;
      for (auto &p : pathLengths) {
        NodeIdentifier a, b;
        int measuredLength = p.second;
        std::tie(a, b) = p.first;
        if (a == b)
          continue;

        int shortestLength = shortestLengths[graph.vertex(a.hex())][graph.vertex(b.hex())];
        double pathStretch = (double) measuredLength / (double) shortestLength;
        averageStretch += pathStretch;
        n++;

        stretch << a.hex()
                << b.hex()
                << shortestLength
                << measuredLength
                << pathStretch;
      }
      averageStretch /= n;

      BOOST_LOG(logger()) << "Average stretch = " << averageStretch;

      // Finish this test
      finish();
    });
  }
};

UNISPHERE_REGISTER_TEST_CASE(AllPairs, "routing/all_pairs")
#endif

class CountState : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /// State dataset
  TestBed::DataSet<> ds_state{"ds_state"};

  /**
   * Count the amount of state a node is using.
   */
  void runNode(TestBed::TestCaseApi &api,
               TestBed::VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    ds_state.add({
      { "node_name",    node->name },
      // Routing table state
      { "rt_all",       node->router->routingTable().size() },
      { "rt_active",    node->router->routingTable().sizeActive() },
      { "rt_vicinity",  node->router->routingTable().sizeVicinity() },
      // Name database state
      { "ndb_all",      node->router->nameDb().size() },
      { "ndb_active",   node->router->nameDb().sizeActive() },
      { "ndb_cache",    node->router->nameDb().sizeCache() }
    });

    finish(api);
  }

  void processLocalResults(TestBed::TestCaseApi &api)
  {
    BOOST_LOG(logger()) << "Sending " << ds_state.size() << " records in ds_state.";
    api.send(ds_state);
  }

  void processGlobalResults(TestBed::TestCaseApi &api)
  {
    api.receive(ds_state);
    BOOST_LOG(logger()) << "Received " << ds_state.size() << " records in ds_state.";

    // TODO: Reporting
  }
};

UNISPHERE_REGISTER_TEST_CASE(CountState, "state/count")

class DumpSloppyGroupTopology : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /// Graph topology type
  typedef SloppyGroupManager::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;
  /// Topology dataset
  TestBed::DataSet<Graph::graph_type> ds_topology{"ds_topology"};

  /**
   * Dump sloppy group topology on each node.
   */
  void runNode(TestBed::TestCaseApi &api,
               TestBed::VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    node->router->sloppyGroup().dumpTopology(graph);
    finish(api);
  }

  void processLocalResults(TestBed::TestCaseApi &api)
  {
    ds_topology.add({ "graph", graph.graph() });
    BOOST_LOG(logger()) << "Sending " << boost::num_vertices(graph.graph()) << " vertices in ds_topology.";
    api.send(ds_topology);
  }

  void processGlobalResults(TestBed::TestCaseApi &api)
  {
    api.receive(ds_topology);
    TestBed::mergeGraphDataset<Graph, SloppyGroupManager::TopologyDumpTags::NodeName>
      (ds_topology, "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    using Tags = SloppyGroupManager::TopologyDumpTags;
    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("is_long", boost::get(Tags::FingerIsLong(), graph.graph()));
    
    TestBed::outputGraphDataset(graph, properties, api.getOutputFilename("sg-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpSloppyGroupTopology, "state/sloppy_group_topology")

#if 0
class DumpRoutingTopology : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /**
   * Dump sloppy group topology in GraphML format.
   */
  void start()
  {
    CompactRoutingTable::TopologyDumpGraph graph;
    boost::dynamic_properties properties;
    properties.property("name", boost::get(CompactRoutingTable::TopologyDumpTags::NodeName(), graph.graph()));
    properties.property("is_landmark", boost::get(CompactRoutingTable::TopologyDumpTags::NodeIsLandmark(), graph.graph()));
    properties.property("state", boost::get(CompactRoutingTable::TopologyDumpTags::NodeStateSize(), graph.graph()));
    properties.property("vport", boost::get(CompactRoutingTable::TopologyDumpTags::LinkVportId(), graph.graph()));

    for (TestBed::VirtualNodePtr node : nodes() | boost::adaptors::map_values) {
      node->router->routingTable().dumpTopology(graph);
    }

    auto topology = data("topology");
    topology << TestBed::DataCollector::Graph<CompactRoutingTable::TopologyDumpGraph::graph_type>{ graph.graph(), properties };

    finish();
  }

  /**
   * This testcase should be run inside a snapshot.
   */
  bool snapshot() { return true; }
};

UNISPHERE_REGISTER_TEST_CASE(DumpRoutingTopology, "state/routing_topology")
#endif

}
