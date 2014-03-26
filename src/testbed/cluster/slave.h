/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_TESTBED_SLAVE_H
#define UNISPHERE_TESTBED_SLAVE_H

#include "testbed/cluster/node.h"

#include <boost/system/error_code.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Testbed node that runs the simulated UNISPHERE nodes.
 */
class UNISPHERE_EXPORT Slave : public ClusterNode {
  friend class SlaveTestCaseApi;
public:
  /**
   * Class constructor.
   */
  Slave();

  Slave(const Slave&) = delete;
  Slave &operator=(const Slave&) = delete;
protected:
  /**
   * Sets up command line options and initializes the cluster node.
   *
   * @param argc Number of command line arguments
   * @param argv Command line arguments
   * @param options Program options parser configuration
   * @param variables Prgoram option variables
   */
  void setupOptions(int argc,
                    char **argv,
                    boost::program_options::options_description &options,
                    boost::program_options::variables_map &variables);

  /**
   * Runs the slave.
   */
  void run();
private:
  /**
   * Performs a testbed cluster join.
   */
  void joinCluster();

  /**
   * Performs a rejoin to a testbed cluster.
   */
  void rejoinCluster();

  /**
   * Performs periodical cluster heartbeats.
   */
  void heartbeat(const boost::system::error_code &error = boost::system::error_code());
private:
  UNISPHERE_DECLARE_PRIVATE(Slave)
};

}

}

#endif
