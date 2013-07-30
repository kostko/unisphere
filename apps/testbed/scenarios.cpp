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

namespace po = boost::program_options;
using namespace UniSphere;

namespace Scenarios {

/**
 * A scenario that does nothing at all.
 */
UNISPHERE_SCENARIO(IdleScenario)
{
  const auto &nodes = api.getNodes();
  for (int i = 0; i <= nodes.size() / 10; i++) {
    api.startNodes(nodes, i*10, 10);
    api.wait(5);
  }
  api.wait(600);

  // TODO: Make the following API for starting nodes in batches of 10 with 5 second delay inbetween
  // api.startNodesBatch(nodes, 10, 5);
}
UNISPHERE_SCENARIO_END_REGISTER(IdleScenario)

UNISPHERE_SCENARIO(StandardTests)
{
  const auto &nodes = api.getNodes();
  for (int i = 0; i <= nodes.size() / 10; i++) {
    api.startNodes(nodes, i*10, 10);
    api.wait(5);
  }
  api.wait(30);
  /// Perform some sanity checks before continuing
  api.test("sanity/check_consistent_ndb");

  // Count routing state after 30 seconds
  api.test("state/count");
  // Dump sloppy group and routing topologies in parallel
  api.wait(30);
  api.test({ "state/sloppy_group_topology", "state/routing_topology" });
  // Perform pair-wise ping test to compute latency and stretch; when
  // profiling is enabled, also collect traces in this section
  api.wait(10);
  api.test("traces/start");
    api.test("routing/pair_wise_ping");
    api.test("traces/retrieve");
  api.test("traces/end");
}
UNISPHERE_SCENARIO_END_REGISTER(StandardTests)

}
