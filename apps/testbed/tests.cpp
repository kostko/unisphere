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
#include <boost/graph/floyd_warshall_shortest.hpp>

using namespace UniSphere;

namespace Tests {

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

    for (TestBed::VirtualNode *a : nodes() | boost::adaptors::map_values) {
      for (TestBed::VirtualNode *b : nodes() | boost::adaptors::map_values) {
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
      for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
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
      TestBed::NodeNameMap &nameMap = names();
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

        stretch << nameMap.right.at(a)
                << nameMap.right.at(b)
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

class CountState : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /**
   * Count the amount of state all nodes are using.
   */
  void start()
  {
    unsigned long stateAllNodes = 0;
    auto state = data("state", {
      "node_id",
      "rt_all", "rt_active", "rt_vicinity",
      "ndb_all", "ndb_active", "ndb_cache",
      "is_landmark"
    });

    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      // Routing table state
      size_t stateRtAll = node->router->routingTable().size();
      size_t stateRtActive = node->router->routingTable().sizeActive();
      size_t stateRtVicinity = node->router->routingTable().sizeVicinity();
      // Name database state
      size_t stateNdbAll = node->router->nameDb().size();
      size_t stateNdbActive = node->router->nameDb().sizeActive();
      size_t stateNdbCache = node->router->nameDb().sizeCache();

      stateAllNodes += stateRtAll + stateNdbAll;
      state
        << node->name
        << stateRtAll
        << stateRtActive
        << stateRtVicinity
        << stateNdbAll
        << stateNdbActive
        << stateNdbCache
        << node->router->routingTable().isLandmark();
    }

    BOOST_LOG(logger()) << "Global state = " << stateAllNodes;
    finish();
  }
};

UNISPHERE_REGISTER_TEST_CASE(CountState, "state/count")

class DumpSloppyGroupTopology : public TestBed::TestCase
{
public:
  using TestBed::TestCase::TestCase;
protected:
  /**
   * Dump sloppy group topology in GraphML format.
   */
  void start()
  {
    SloppyGroupManager::TopologyDumpGraph graph;
    boost::dynamic_properties properties;
    properties.property("name", boost::get(SloppyGroupManager::TopologyDumpTags::NodeName(), graph.graph()));
    properties.property("is_long", boost::get(SloppyGroupManager::TopologyDumpTags::FingerIsLong(), graph.graph()));

    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      node->router->sloppyGroup().dumpTopology(graph,
        [&](const NodeIdentifier &n) -> std::string { return boost::replace_all_copy(names().right.at(n), " ", "_"); });
    }

    auto topology = data("topology");
    topology << TestBed::DataCollector::Graph<SloppyGroupManager::TopologyDumpGraph::graph_type>{ graph.graph(), properties };

    finish();
  }

  /**
   * This testcase should be run inside a snapshot.
   */
  bool snapshot() { return true; }
};

UNISPHERE_REGISTER_TEST_CASE(DumpSloppyGroupTopology, "state/sloppy_group_topology")

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

    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      node->router->routingTable().dumpTopology(graph,
        [&](const NodeIdentifier &n) -> std::string { return boost::replace_all_copy(names().right.at(n), " ", "_"); });
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

}
