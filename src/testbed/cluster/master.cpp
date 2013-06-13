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
#include "testbed/cluster/master.h"
#include "testbed/test_bed.h"
#include "testbed/exceptions.h"
#include "identity/node_identifier.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"

#include <unordered_map>
#include <sstream>
#include <boost/range/adaptors.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

class SlaveDescriptor {
public:
  NodeIdentifier nodeId;
  // TODO: Assigned virtual nodes
};

class MasterPrivate {
public:
  // TODO
public:
  /// Registered slaves
  std::unordered_map<NodeIdentifier, SlaveDescriptor> m_slaves;
};

Master::Master()
  : ClusterNode(),
    d(new MasterPrivate)
{
}

void Master::setupOptions(int argc,
                          char **argv,
                          po::options_description &options,
                          po::variables_map &variables)
{
  TestBed &testbed = TestBed::getGlobalTestbed();
  
  if (variables.empty()) {
    ClusterNode::setupOptions(argc, argv, options, variables);

    // Generate a list of all available scenarios
    std::stringstream scenarios;
    scenarios << "scenario to run\n"
              << "\n"
              << "Available scenarios:\n";

    for (ScenarioPtr scenario : testbed.scenarios() | boost::adaptors::map_values) {
      scenarios << "  " << scenario->name() << "\n";
    }

    // Testbed options on master node
    po::options_description local("Testbed Options");
    local.add_options()
      ("scenario", po::value<std::string>(), scenarios.str().data())
      ("out-dir", po::value<std::string>(), "directory for output files")
//      ("id-gen", po::value<IdGenerationType>(), "id generation type [random, consistent]")
      ("seed", po::value<std::uint32_t>()->default_value(0), "seed for the basic RNG")
      ("max-runtime", po::value<unsigned int>()->default_value(0), "maximum runtime in seconds (0 = unlimited)")
    ;
    options.add(local);
    return;
  }

  // Process local options
  ClusterNode::setupOptions(argc, argv, options, variables);

  // TODO: Process testbed options
  ScenarioPtr scenario;
  if (variables.count("scenario")) {
    scenario = testbed.getScenario(variables["scenario"].as<std::string>());
    if (!scenario)
      throw ArgumentError("The specified scenario is not registered!");
  } else {
    throw ArgumentError("Missing required --scenario option!");
  }

  scenario->initialize(argc, argv, options);
}

void Master::run()
{
  context().logger()
    << Logger::Component{"Master"}
    << Logger::Level::Info
    << "Cluster master initialized (id=" << linkManager().getLocalContact().nodeId().hex() << ")." << std::endl;
}

}

}
