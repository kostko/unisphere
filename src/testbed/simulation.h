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
#ifndef UNISPHERE_TESTBED_SIMULATION_H
#define UNISPHERE_TESTBED_SIMULATION_H

#include "interplex/contact.h"

#include <boost/signals2/signal.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Simulation instance.
 */
class UNISPHERE_EXPORT Simulation : public boost::enable_shared_from_this<Simulation> {
public:
  /**
   * Simulation state.
   */
  enum class State {
    /// Simulation is currently stopped
    Stopped,
    /// Simulation is running
    Running,
    /// Simulation is stopping
    Stopping
  };

  /**
   * Constructs a simulation object.
   *
   * @param seed Seed for the basic RNG
   * @param threads Number of threads for this simulation
   * @param globalNodeCount Number of all nodes in the simulation (whole cluster)
   */
  Simulation(std::uint32_t seed,
             size_t threads,
             size_t globalNodeCount);

  /**
   * Creates a new virtual node inside the simulation.
   *
   * @param name Virtual node name
   * @param contact Node contact information (bindable to local interface)
   * @param peers A list of peers in the social network
   */
  void createNode(const std::string &name,
                  const Contact &contact,
                  const std::list<Contact> &peers);

  /**
   * Returns the current simulation state.
   */
  State state() const;

  /**
   * Returns true if the simulation is running.
   */
  bool isRunning() const;

  /**
   * Returns true if the simulation is stopping.
   */
  bool isStopping() const;

  /**
   * Starts the simulation.
   */
  void run();

  /**
   * Request the simulation to stop. Note that the simulation may not
   * stop immediately -- to get notified when the simulation stops, subscribe
   * to the signalStopped signal.
   */
  void stop();
public:
  /// Signal that gets emitted after simulation has stopped; note that the signal
  /// is called from the simulation control thread.
  boost::signals2::signal<void()> signalStopped;
private:
  UNISPHERE_DECLARE_PRIVATE(Simulation)
};

UNISPHERE_SHARED_POINTER(Simulation)

}

}

#endif
