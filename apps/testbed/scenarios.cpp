/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#include "testbed/exceptions.h"

#include "tests.cpp"

namespace po = boost::program_options;
using namespace UniSphere;
using namespace UniSphere::TestBed;

namespace Scenarios {

/**
 * A scenario that does nothing at all.
 */
UNISPHERE_SCENARIO(IdleScenario)
{
  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  api.wait(30);
  api.mark("all_nodes_up");

  api.wait(3570);

  // Perform some sanity checks
  api.test("sanity/check_consistent_ndb");
  // Retrieve performance statistics
  api.test("stats/performance");
}
UNISPHERE_SCENARIO_END_REGISTER(IdleScenario)

UNISPHERE_SCENARIO(StandardTests)
{
  // Start collecting performance data
  TestCasePtr perfCollector = api.testInBackground("stats/collect_performance");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  auto standardTests = [&]() {
    // Perform some sanity checks
    api.test("sanity/check_consistent_ndb");
    // Dump topology information
    api.test({ "state/sloppy_group_topology", "state/routing_topology" });
    // Retrieve performance statistics
    api.test("stats/performance");
    // Retrieve L-R address length distribution
    api.test("stats/lr_address_lengths");
  };

  api.wait(30);
  api.mark("all_nodes_up");

  standardTests();
  api.wait(570);
  standardTests();

  // Collect link congestion information while pinging
  auto linkCollector = api.testInBackground<Tests::CollectLinkCongestion>("stats/collect_link_congestion");
  linkCollector->pairWisePing = api.test<Tests::PairWisePing>(
    "routing/pair_wise_ping",
    {{ "destinations_per_node", 2 }}
  );
  api.signal(linkCollector, "finish");

  api.wait(600);

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(StandardTests)

UNISPHERE_SCENARIO(Churn)
{
  // Start collecting performance data
  TestCasePtr perfCollector = api.testInBackground("stats/collect_performance");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  api.wait(30);
  api.mark("all_nodes_up");

  // Run for another 270 seconds without interruptions
  api.wait(270);
  api.test({ "state/sloppy_group_topology", "state/routing_topology" });

  // Obtain the routing topology
  auto rtTopology = api.test<Tests::DumpRoutingTopology>("state/routing_topology");
  const auto &rtGraph = rtTopology->graph;

  // Select some nodes to terminate
  using Tags = UniSphere::CompactRoutingTable::TopologyDumpTags;
  auto &rng = api.rng();

  api.mark("churn_start");
  for (auto vp = boost::vertices(rtGraph); vp.first != vp.second; ++vp.first) {
    bool isLandmark = boost::get(Tags::NodeIsLandmark(), rtGraph.graph(), *vp.first);

    // Skip landmark nodes from being terminated
    // TODO: Make this configurable
    if (isLandmark)
      continue;
    // Terminate ~5% of nodes in total
    if (std::generate_canonical<double, 10>(rng) < 0.95)
      continue;

    NodeIdentifier nodeId(boost::get(Tags::NodeName(), rtGraph.graph(), *vp.first), NodeIdentifier::Format::Hex);
    api.stopNode(nodeId);
    api.wait(15);

    // Dump topology information
    api.test({ "state/sloppy_group_topology", "state/routing_topology" });
  }
  api.mark("churn_end");

  // Run for another 270 seconds without interruptions
  api.wait(270);
  api.test({ "state/sloppy_group_topology", "state/routing_topology" });

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(Churn)

UNISPHERE_SCENARIO(SybilNodesNames)
{
  // Start collecting performance data
  TestCasePtr perfCollector = api.testInBackground("stats/collect_performance");

  // Configure Sybil nodes to be malicious
  TestCasePtr sybils = api.testInBackground("roles/setup_sybil_nodes");
  api.signal(sybils, "evil_names");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  api.wait(30);
  api.mark("all_nodes_up");

  api.wait(90);

  // Perform some sanity checks
  api.test("sanity/check_consistent_ndb", {{ "sybil_mode", true }});
  // Dump topology information
  api.test({ "state/sloppy_group_topology", "state/routing_topology" });

  api.wait(10);

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
  // Stop sybil behaviour
  api.signal(sybils, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(SybilNodesNames)

UNISPHERE_SCENARIO(SybilNodesRouting)
{
  // Start collecting performance data
  TestCasePtr perfCollector = api.testInBackground("stats/collect_performance");

  // Configure Sybil nodes to be malicious
  TestCasePtr sybils = api.testInBackground("roles/setup_sybil_nodes");
  api.signal(sybils, "evil_names");
  api.signal(sybils, "evil_routing");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  api.wait(30);
  api.mark("all_nodes_up");

  api.wait(90);

  // Perform some sanity checks
  api.test("sanity/check_consistent_ndb", {{ "sybil_mode", true }});
  // Dump topology information
  api.test({ "state/sloppy_group_topology", "state/routing_topology" });

  api.wait(10);

  // Check pair-wise connectivity
  api.test("routing/pair_wise_ping", {
    { "sybil_mode", true },
    { "community_limit", true },
    { "destinations_per_node", 2 }
  });

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
  // Stop sybil behaviour
  api.signal(sybils, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(SybilNodesRouting)

}
