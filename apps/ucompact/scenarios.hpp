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

using namespace UniSphere;

namespace Scenarios {

class SimpleTestScenario : public TestBed::Scenario
{
public:
  void setup()
  {
    testbed.loadTopology("../data/social_topology.dat");

    // Dump all state after 80 seconds
    testbed.scheduleTest(80, "state/dump_all");

    // Schedule first test after 85 seconds, further tests each 45 seconds
    testbed.scheduleCall(85, [&]() {
      testbed.runTest("routing/all_pairs");
      testbed.scheduleTestEvery(45, "routing/all_pairs");
    });

    // Terminate tests after 3600 seconds
    testbed.endScenarioAfter(3600);
  }
};

}
