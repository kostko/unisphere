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
#include "scenarios.hpp"

using namespace UniSphere;

int main(int argc, char **argv)
{
  TestBed::TestBed &testbed = TestBed::TestBed::getGlobalTestbed();
  testbed.setupPhyNetwork("127.42.0.1", 8472);
  testbed.registerScenario(new Scenarios::SimpleTestScenario);
  testbed.runScenario("SimpleTestScenario");
  testbed.run();
  return 0;
}
