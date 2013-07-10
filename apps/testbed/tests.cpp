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
#include "testbed/dataset/csv.hpp"
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
using namespace UniSphere::TestBed;

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

class PairWisePing : public TestCase
{
public:
  using TestCase::TestCase;
protected:
  /// Raw measurements measurements
  DataSet<> ds_raw{"ds_raw"};
  /// Node sampling
  std::uniform_real_distribution<double> sampler{0.0, 1.0};
  /// Mutex
  std::recursive_mutex mutex;
  /// Nodes pending RPC call
  std::list<std::function<void()>> pending;

  SelectedPartition::Node selectNode(const Partition &partition,
                                     const Partition::Node &node,
                                     TestCaseApi &api)
  {
    boost::property_tree::ptree args;

    // Specify identifiers that should be paired with this node
    for (const Partition &p : api.getPartitions()) {
      for (const Partition::Node &n : p.nodes) {
        if (n.contact.nodeId() == node.contact.nodeId())
          continue;

        // Sample a subset of nodes
        // TODO: Make this subset configurable per test case instance
        if (sampler(api.rng()) < 0.8)
          continue;

        args.add("nodes.node", n.contact.nodeId().hex());
      }
    }

    return SelectedPartition::Node{ node.contact.nodeId(), args };
  }

  void callNext(TestCaseApi &api)
  {
    RecursiveUniqueLock lock(mutex);

    if (pending.empty())
      return finish(api);

    auto next = pending.front();
    pending.pop_front();
    api.defer(next);
  }

  /**
   * Perform all-pairs reachability testing.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    RecursiveUniqueLock lock(mutex);
    RpcEngine<SocialRpcChannel> &rpc = node->router->rpcEngine();

    // We should test one node at a time, to prevent overloading the network
    for (const auto &p : args.get_child("nodes")) {
      NodeIdentifier nodeId(p.second.data(), NodeIdentifier::Format::Hex);
      pending.push_back([this, &api, &rpc, node, nodeId]() {
        using namespace boost::posix_time;
        ptime xmitTime = microsec_clock::universal_time();
        Protocol::PingRequest request;
        request.set_timestamp(1);

        rpc.call<Protocol::PingRequest, Protocol::PingResponse>(nodeId, "Core.Ping", request,
          [this, &api, node, nodeId, xmitTime](const Protocol::PingResponse &rsp, const RoutedMessage &msg) {
            ds_raw.add({
              { "node_a", node->nodeId.hex() },
              { "node_b", nodeId.hex() },
              { "success", true },
              { "hops", rsp.hopcount() - msg.hopCount() },
              { "rtt", (microsec_clock::universal_time() - xmitTime).total_milliseconds() }
            });
            callNext(api);
          },
          [this, &api, node, nodeId](RpcErrorCode code, const std::string &msg) {
            ds_raw.add({
              { "node_a", node->nodeId.hex() },
              { "node_b", nodeId.hex() },
              { "success", false }
            });
            callNext(api);
          },
          rpc.options().setTimeout(15)
        );
      });
    }
  }

  void localNodesRunning(TestCaseApi &api)
  {
    // Start executing ping calls
    BOOST_LOG(logger()) << "Pinging " << pending.size() << " node pairs.";
    callNext(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    BOOST_LOG(logger()) << "Ping calls completed.";
    api.send(ds_raw);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_raw);

    outputCsvDataset(
      ds_raw,
      { "node_a", "node_b", "success", "hops", "rtt" },
      api.getOutputFilename("raw", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(PairWisePing, "routing/pair_wise_ping")

class CountState : public TestCase
{
public:
  using TestCase::TestCase;
protected:
  /// State dataset
  DataSet<> ds_state{"ds_state"};

  /**
   * Count the amount of state a node is using.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
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

  void processLocalResults(TestCaseApi &api)
  {
    BOOST_LOG(logger()) << "Sending " << ds_state.size() << " records in ds_state.";
    api.send(ds_state);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_state);
    BOOST_LOG(logger()) << "Received " << ds_state.size() << " records in ds_state.";

    outputCsvDataset(
      ds_state,
      { "node_name", "rt_all", "rt_active", "rt_vicinity", "ndb_all", "ndb_active", "ndb_cache" },
      api.getOutputFilename("state", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(CountState, "state/count")

class DumpSloppyGroupTopology : public TestCase
{
public:
  using TestCase::TestCase;
protected:
  /// Graph topology type
  typedef SloppyGroupManager::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;
  /// Topology dataset
  DataSet<Graph::graph_type> ds_topology{"ds_topology"};

  /**
   * Dump sloppy group topology on each node.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    node->router->sloppyGroup().dumpTopology(graph);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    ds_topology.add({ "graph", graph.graph() });
    BOOST_LOG(logger()) << "Sending " << boost::num_vertices(graph.graph()) << " vertices in ds_topology.";
    api.send(ds_topology);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = SloppyGroupManager::TopologyDumpTags;

    api.receive(ds_topology);
    mergeGraphDataset<Graph, Tags::NodeName>(ds_topology, "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("is_long", boost::get(Tags::FingerIsLong(), graph.graph()));
    
    outputGraphDataset(graph, properties, api.getOutputFilename("sg-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpSloppyGroupTopology, "state/sloppy_group_topology")

class DumpRoutingTopology : public TestCase
{
public:
  using TestCase::TestCase;
protected:
  /// Graph topology type
  typedef CompactRoutingTable::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;
  /// Topology dataset
  DataSet<Graph::graph_type> ds_topology{"ds_topology"};

  /**
   * Dump routing topology on each node.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    node->router->routingTable().dumpTopology(graph);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    ds_topology.add({ "graph", graph.graph() });
    BOOST_LOG(logger()) << "Sending " << boost::num_vertices(graph.graph()) << " vertices in ds_topology.";
    api.send(ds_topology);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = CompactRoutingTable::TopologyDumpTags;

    api.receive(ds_topology);
    mergeGraphDataset<Graph, Tags::NodeName>(ds_topology, "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("is_landmark", boost::get(Tags::NodeIsLandmark(), graph.graph()));
    properties.property("state", boost::get(Tags::NodeStateSize(), graph.graph()));
    properties.property("vport", boost::get(Tags::LinkVportId(), graph.graph()));
    
    outputGraphDataset(graph, properties, api.getOutputFilename("rt-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpRoutingTopology, "state/routing_topology")

}
