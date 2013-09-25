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
  // Start collecting performance data
  TestCasePtr testPerfCollector = api.testInBackground("stats/collect_performance");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  auto standardTests = [&]() {
    // Perform some sanity checks
    api.test("sanity/check_consistent_ndb");
    // Dump topology information
    api.test({ "state/sloppy_group_topology", "state/routing_topology" });
    // Retrieve performance statistics
    api.test("stats/performance");
  };

  api.wait(30);
  standardTests();
  api.wait(570);
  standardTests();
  api.wait(600);

  // Stop collecting performance data
  api.signal(testPerfCollector, "finish");
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
  };

  api.wait(30);
  standardTests();
  api.wait(570);
  standardTests();

  // Collect link congestion information while pinging
  auto linkCollector = api.testInBackground<Tests::CollectLinkCongestion>("stats/collect_link_congestion");
  linkCollector->pairWisePing = api.test<Tests::PairWisePing>("routing/pair_wise_ping");
  api.signal(linkCollector, "finish");
  
  api.wait(600);

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(StandardTests)

UNISPHERE_SCENARIO(SybilNodes)
{
  // Start collecting performance data
  TestCasePtr perfCollector = api.testInBackground("stats/collect_performance");

  // Configure Sybil nodes to be malicious
  TestCasePtr sybils = api.testInBackground("roles/setup_sybil_nodes");
  api.signal(sybils, "evil");

  // Start nodes in batches
  api.startNodesBatch(api.getNodes(), 10, 5);

  api.wait(30);
  // Perform some sanity checks
  api.test("sanity/check_consistent_ndb", {{ "sybil_mode", true }});
  
  api.wait(300);

  // Stop collecting performance data
  api.signal(perfCollector, "finish");
  // Stop sybil behaviour
  api.signal(sybils, "finish");
}
UNISPHERE_SCENARIO_END_REGISTER(SybilNodes)

}
