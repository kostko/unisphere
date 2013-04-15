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

UNISPHERE_SCENARIO(SimpleTestScenario)
{
  if (options.count("topology"))
    testbed.loadTopology(options["topology"].as<std::string>());
  else
    throw TestBed::TestBedException("Missing required --topology option!");

  // Dump all state after 80 seconds
  //testbed.scheduleTest(80, "state/dump_all");

  // Dump state statistics every 30 seconds
  testbed.scheduleTestEvery(30, "state/count");

  // Dump sloppy group topology
  testbed.scheduleTestEvery(30, "state/sloppy_group_topology");

  // Dump routing topology
  testbed.scheduleTestEvery(30, "state/routing_topology");

  // Schedule first test after 85 seconds, further tests each 45 seconds
  testbed.scheduleCall(85, [&]() {
    testbed.runTest("routing/all_pairs");
    testbed.scheduleTestEvery(45, "routing/all_pairs");
  });

  // Terminate tests after 3600 seconds
  testbed.endScenarioAfter(3600);
}

void setupOptions(boost::program_options::options_description &options)
{
  options.add_options()
    ("topology", po::value<std::string>(), "topology file in GraphML format")
  ;
}
UNISPHERE_SCENARIO_END

UNISPHERE_SCENARIO(IdleScenario)
{
  if (options.count("topology"))
    testbed.loadTopology(options["topology"].as<std::string>());
  else
    throw TestBed::TestBedException("Missing required --topology option!");

  // Terminate tests after 3600 seconds
  testbed.endScenarioAfter(3600);
}

void setupOptions(boost::program_options::options_description &options)
{
  options.add_options()
    ("topology", po::value<std::string>(), "topology file in GraphML format")
  ;
}
UNISPHERE_SCENARIO_END

}
