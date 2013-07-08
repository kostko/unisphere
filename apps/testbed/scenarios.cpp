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
}
UNISPHERE_SCENARIO_END_REGISTER(IdleScenario)

UNISPHERE_SCENARIO(StandardTests)
{
  // Count routing state after 30 seconds
  api.runTestCaseAt(30, "state/count");
  // Dump sloppy group topology after 60 seconds
  api.runTestCaseAt(60, "state/sloppy_group_topology");
  // Dump routing topology after 60 seconds
  api.runTestCaseAt(60, "state/routing_topology");
}
UNISPHERE_SCENARIO_END_REGISTER(StandardTests)

}
