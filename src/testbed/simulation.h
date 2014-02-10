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
#ifndef UNISPHERE_TESTBED_SIMULATION_H
#define UNISPHERE_TESTBED_SIMULATION_H

#include "interplex/contact.h"
#include "testbed/nodes.h"

#include <boost/signals2/signal.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace UniSphere {

namespace TestBed {

class Simulation;

/**
 * A section of scheduled executions that should be run inside the
 * simulation.
 */
class UNISPHERE_EXPORT SimulationSection : public boost::enable_shared_from_this<SimulationSection> {
  friend class Simulation;
  friend class SimulationPrivate;
public:
  /// Section function type
  typedef std::function<void()> SectionFunction;
  /// Section function with node type
  typedef std::function<void(VirtualNodePtr)> SectionFunctionNode;
public:
  SimulationSection(const SimulationSection&) = delete;
  SimulationSection &operator=(const SimulationSection&) = delete;

  /**
   * Schedules a specific function to be executed inside the simulation on
   * for a specific node.
   *
   * @param nodeId Virtual node identifier
   * @param fun Function to be executed
   */
  void execute(const NodeIdentifier &nodeId, SectionFunctionNode fun);

  /**
   * Schedules a specific function to be executed inside the simulation.
   *
   * @param fun Function to be executed
   */
  void execute(SectionFunction fun);

  /**
   * Starts executing all the scheduled functions. The functions are run inside
   * the simulation thread. If the simulation is not running, this method does
   * nothing.
   */
  void run();

  /**
   * Schedules the section to be executed after some timeout.
   *
   * @param timeout Timeout in seconds
   */
  void schedule(int timeout);
public:
  /// Signal that gets called when all pending functions have executed
  boost::signals2::signal<void()> signalFinished;
private:
  /**
   * Private constructor.
   */
  explicit SimulationSection(Simulation &simulation);
private:
  UNISPHERE_DECLARE_PRIVATE(SimulationSection)
};

UNISPHERE_SHARED_POINTER(SimulationSection)

/**
 * Simulation instance.
 */
class UNISPHERE_EXPORT Simulation : public boost::enable_shared_from_this<Simulation> {
  friend class SimulationSection;
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

  Simulation(const Simulation&) = delete;
  Simulation &operator=(const Simulation&) = delete;

  /**
   * Creates a new section of execution.
   */
  SimulationSectionPtr section();

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
   * Returns the simulation's random seed.
   */
  std::uint32_t seed() const;

  /**
   * Returns the simulation context.
   */
  const Context &context() const;

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
