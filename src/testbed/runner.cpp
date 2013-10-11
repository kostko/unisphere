/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "testbed/cluster/node.h"
#include "testbed/cluster/master.h"
#include "testbed/cluster/slave.h"
#include "testbed/cluster/controller.h"
#include "testbed/exceptions.h"

#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

/**
 * Cluster role.
 */
enum class ClusterRole {
  /// Node is a master and coordinates the cluster
  Master,
  /// Node is a slave and performs the simulations
  Slave,
  /// Node is a controller and submits commands to master
  Controller
};

std::istream &operator>>(std::istream &is, ClusterRole &role)
{
  std::string token;
  is >> token;
  if (token == "master")
    role = ClusterRole::Master;
  else if (token == "slave")
    role = ClusterRole::Slave;
  else if (token == "controller")
    role = ClusterRole::Controller;
  else
    throw boost::program_options::invalid_option_value(token);
  return is;
}

class RunnerPrivate {
public:
  RunnerPrivate();

  int run(int argc, char **argv);
public:
  /// Cluster node instance
  ClusterNodePtr m_clusterNode;
};

RunnerPrivate::RunnerPrivate()
{
}

int RunnerPrivate::run(int argc, char **argv)
{
  // Setup program options
  po::options_description options;

  po::options_description baseOptions("Base options");
  baseOptions.add_options()
    ("help", "displays help information")
    ("cluster-role", po::value<ClusterRole>(), "cluster role (master, slave, controller)")
  ;
  options.add(baseOptions);

  // Parse options
  po::variables_map vm;
  try {
    auto globalParsed = po::command_line_parser(argc, argv).options(baseOptions).allow_unregistered().run();
    po::store(globalParsed, vm);
    po::notify(vm);

    // Handle cluster role option before all others as it determines which module will be used
    if (vm.count("cluster-role")) {
      switch (vm["cluster-role"].as<ClusterRole>()) {
        case ClusterRole::Master: m_clusterNode = ClusterNodePtr(new Master); break;
        case ClusterRole::Slave: m_clusterNode = ClusterNodePtr(new Slave); break;
        case ClusterRole::Controller: m_clusterNode = ClusterNodePtr(new Controller); break;
      }
    } else {
      std::cout << "ERROR: No --cluster-role specified!" << std::endl;
      std::cout << options << std::endl;
      return 1;
    }

    m_clusterNode->initialize(argc, argv, options);
    if (vm.count("help")) {
      std::cout << "UNISPHERE Testbed" << std::endl;
      std::cout << options << std::endl;
      return 1;
    }

    return m_clusterNode->start();
  } catch (ArgumentError &e) {
    std::cout << "ERROR: There is an error in your invocation arguments!" << std::endl;
    std::cout << "ERROR: " << e.message() << std::endl;
    std::cout << options << std::endl;
    return 1;
  } catch (boost::program_options::error &e) {
    std::cout << "ERROR: There is an error in your invocation arguments!" << std::endl;
    std::cout << "ERROR: " << e.what() << std::endl;
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
