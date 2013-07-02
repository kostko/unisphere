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
#include "testbed/simulation.h"
#include "testbed/exceptions.h"
#include "core/context.h"
#include "social/size_estimator.h"
#include "social/social_identity.h"

#include <boost/range/adaptors.hpp>

namespace UniSphere {

namespace TestBed {

class SimulationPrivate;

class SimulationSectionPrivate {
public:
  SimulationSectionPrivate(SimulationPrivate &simulation);
public:
  /// Simulation instance
  SimulationPrivate &m_simulation;
  /// Scheduled functions
  std::list<std::function<void()>> m_queue;
};

class SimulationPrivate {
public:
  SimulationPrivate(std::uint32_t seed, size_t threads, size_t globalNodeCount);

  void runSections();
public:
  /// Mutex
  std::recursive_mutex m_mutex;
  /// Current simulation state
  Simulation::State m_state;
  /// Internal simulation thread
  boost::thread m_thread;
  /// Simulation context
  Context m_context;
  /// Size estimator
  OracleNetworkSizeEstimator m_sizeEstimator;
  /// Virtual nodes
  VirtualNodeMap m_nodes;
  /// Simulation seed
  std::uint32_t m_seed;
  /// Number of threads
  size_t m_threads;
  /// Sections pending execution
  std::list<SimulationSectionPtr> m_pendingSections;
};

SimulationSectionPrivate::SimulationSectionPrivate(SimulationPrivate &simulation)
  : m_simulation(simulation)
{
}

SimulationSection::SimulationSection(Simulation &simulation)
  : d(new SimulationSectionPrivate(*simulation.d))
{
}

void SimulationSection::execute(const NodeIdentifier &nodeId,
                                SectionFunction fun)
{
  RecursiveUniqueLock lock(d->m_simulation.m_mutex);
  auto it = d->m_simulation.m_nodes.find(nodeId);
  if (it == d->m_simulation.m_nodes.end())
    throw VirtualNodeNotFound(nodeId);


  d->m_queue.push_back(boost::bind(fun, it->second));
}

void SimulationSection::run()
{
  RecursiveUniqueLock lock(d->m_simulation.m_mutex);
  if (d->m_simulation.m_state != Simulation::State::Running)
    return;

  d->m_simulation.m_pendingSections.push_back(shared_from_this());
  lock.unlock();

  // Interrupt the simulation thread to invoke the section runner
  d->m_simulation.m_thread.interrupt();
}

SimulationPrivate::SimulationPrivate(std::uint32_t seed, size_t threads, size_t globalNodeCount)
  : m_sizeEstimator(globalNodeCount),
    m_seed(seed),
    m_threads(threads)
{
  m_context.signalInterrupted.connect(boost::bind(&SimulationPrivate::runSections, this));
}

void SimulationPrivate::runSections()
{
  std::list<SimulationSectionPtr> sections;
  {
    RecursiveUniqueLock lock(m_mutex);
    sections = std::move(m_pendingSections);
    m_pendingSections.clear();
  }

  for (SimulationSectionPtr section : sections) {
    for (std::function<void()> fun : section->d->m_queue) {
      fun();
    }

    section->signalFinished();
  }
}

Simulation::Simulation(std::uint32_t seed, size_t threads, size_t globalNodeCount)
  : d(new SimulationPrivate(seed, threads, globalNodeCount))
{
}

SimulationSectionPtr Simulation::section()
{
  return SimulationSectionPtr(new SimulationSection(*this));
}

void Simulation::createNode(const std::string &name,
                            const Contact &contact,
                            const std::list<Contact> &peers)
{
  RecursiveUniqueLock lock(d->m_mutex);
  VirtualNodePtr node = VirtualNodePtr(new VirtualNode(d->m_context, d->m_sizeEstimator, name, contact));
  for (const Contact &peer : peers) {
    node->identity->addPeer(peer);
  }

  d->m_nodes.insert({{ contact.nodeId(), node }});
}

Simulation::State Simulation::state() const
{
  return d->m_state;
}

bool Simulation::isRunning() const
{
  return d->m_state == Simulation::State::Running;
}

bool Simulation::isStopping() const
{
  return d->m_state == Simulation::State::Stopping;
}

void Simulation::run()
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_state = State::Running;

  // Create a shared instance of ourselves and store it in the thread; this
  // is to ensure that the simulation doesn't get destroyed until the context
  // is running.
  SimulationPtr self = shared_from_this();

  // Setup initializer to seed the basic RNGs
  d->m_context.setThreadInitializer([this](){
    d->m_context.basicRng().seed(d->m_seed);
  });

  d->m_thread = std::move(boost::thread([this, self]() {
    // Seed the basic RNG
    d->m_context.basicRng().seed(d->m_seed);

    // Initialize all virtual nodes
    {
      RecursiveUniqueLock lock(d->m_mutex);
      for (VirtualNodePtr vnode : d->m_nodes | boost::adaptors::map_values) {
        vnode->initialize();
      }
    }

    // Run the simulated context
    d->m_context.run(d->m_threads);

    // After the context completes we emit the simulation stopped signal
    d->m_state = State::Stopped;
    self->signalStopped();
  }));
}

void Simulation::stop()
{
  RecursiveUniqueLock lock(d->m_mutex);
  if (d->m_state == State::Stopped)
    return;

  // Request the context to stop and switch states
  d->m_state = State::Stopping;
  d->m_context.stop();
}

}

}
