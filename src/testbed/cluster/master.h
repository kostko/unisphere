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
#ifndef UNISPHERE_TESTBED_MASTER_H
#define UNISPHERE_TESTBED_MASTER_H

#include "testbed/cluster/node.h"

namespace UniSphere {

namespace TestBed {

/**
 * Testbed cluster master node responsible for keeping a list of active
 * slaves and providing this information to the controller node.
 */
class UNISPHERE_EXPORT Master : public ClusterNode {
public:
  /**
   * State of the master.
   */
  enum class State {
    /// In idle state, the master is accepting new slaves
    Idle,
    /// After the simulation has started nodes are no longer accepted
    Running,
    /// Simulation is being aborted
    Aborting
  };

  /**
   * Class constructor.
   */
  Master();

  Master(const Master&) = delete;
  Master &operator=(const Master&) = delete;
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
   * Runs the master.
   */
  void run();
private:
  UNISPHERE_DECLARE_PRIVATE(Master)
};

}

}

#endif
