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
#include "testbed/runner.h"
#include "testbed/test_bed.h"
#include "testbed/cluster/node.h"

#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

class RunnerPrivate {
public:
  RunnerPrivate();

  int run(int argc, char **argv);
public:
  /// Cluster node instance
  ClusterNode *m_clusterNode;
};

RunnerPrivate::RunnerPrivate()
  : m_clusterNode(nullptr)
{
}

int RunnerPrivate::run(int argc, char **argv)
{
  // Setup program options
  po::options_description options;
  po::options_description localOptions;

  po::options_description baseOptions("Base options");
  baseOptions.add_options()
    ("help", "show help message")
  ;
  options.add(baseOptions);
  localOptions.add(baseOptions);

  po::options_description clusterOptions("Cluster options");
  clusterOptions.add_options()
    // TODO change to custom enumeration
    ("cluster-role", po::value<std::string>()->default_value("single"), "determine cluster role")
  ;
  options.add(clusterOptions);
  localOptions.add(clusterOptions);

  TestBed &testbed = TestBed::getGlobalTestbed();
  testbed.addProgramOptions(options);

  // Parse options
  po::variables_map vm;
  try {
    auto globalParsed = po::command_line_parser(argc, argv).options(localOptions).allow_unregistered().run();
    po::store(globalParsed, vm);
    po::notify(vm);

    // Handle help option before others
    if (vm.count("help")) {
      // Handle help option
      std::cout << "UNISPHERE Testbed" << std::endl;
      std::cout << options << std::endl;
      return 1;
    }

    // Handle cluster setup options
    // TODO

    // Handle testbed program options 
    int ret = testbed.parseProgramOptions(argc, argv);
    if (ret > 0) {
      std::cout << options << std::endl;
      return ret;
    }
  } catch (std::exception &e) {
    std::cout << "ERROR: There is an error in your invocation arguments!" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

  return 0;
}

Runner::Runner()
  : d(new RunnerPrivate)
{
}

int Runner::run(int argc, char **argv)
{
  return d->run(argc, argv);
}

}

}
