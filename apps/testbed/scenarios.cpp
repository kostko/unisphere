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
 * A scenario that performs mixed tests at various intervals.
 */
UNISPHERE_SCENARIO(IdleScenario)
{
  // TODO
}

void setupOptions(po::options_description &options,
                  po::variables_map &variables)
{
  if (variables.empty()) {
    options.add_options()
      ("topology", po::value<std::string>(), "topology file in GraphML format")
    ;
    return;
  }

  // Validate the options
  if (!variables.count("topology"))
    throw TestBed::ArgumentError("Missing required --topology option!");
}
UNISPHERE_SCENARIO_END_REGISTER(IdleScenario)

}