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
#include "testbed/simulation.h"
#include "testbed/exceptions.h"
#include "testbed/test_case.h"
#include "core/context.h"
#include "core/consumer_thread.h"
#include "social/size_estimator.h"
#include "social/social_identity.h"

#include <boost/make_shared.hpp>
#include <boost/range/adaptors.hpp>

namespace UniSphere {

namespace TestBed {

class SimulationPrivate;

class SimulationSectionPrivate {
public:
  SimulationSectionPrivate(SimulationPrivate &simulation, TestCasePtr testCase);
public:
  /// Simulation instance
  SimulationPrivate &m_simulation;
  /// Test case we are executing for
  TestCasePtr m_testCase;
  /// Scheduled functions
  std::list<std::function<void()>> m_queue;
};

class SimulationWorkerThread : public ConsumerThread<SimulationSectionPtr> {
public:
  virtual void consume(SimulationSectionPtr &&section) const override
  {
    for (const auto &fun : section->d->m_queue) {
      fun();
    }

    section->signalFinished();
  }
};

class SimulationPrivate {
public:
  SimulationPrivate(std::uint32_t seed, size_t threads, size_t globalNodeCount);
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
  /// Section worker threads
  std::unordered_map<TestCase::Identifier, SimulationWorkerThread> m_sectionThreads;
};

SimulationSectionPrivate::SimulationSectionPrivate(SimulationPrivate &simulation, TestCasePtr testCase)
  : m_simulation(simulation),
    m_testCase(testCase)
{
}

SimulationSection::SimulationSection(Simulation &simulation, TestCasePtr testCase)
  : d(new SimulationSectionPrivate(*simulation.d, testCase))
{
}

void SimulationSection::execute(const NodeIdentifier &nodeId,
                                SectionFunctionNode fun)
{
  RecursiveUniqueLock lock(d->m_simulation.m_mutex);
  auto it = d->m_simulation.m_nodes.find(nodeId);
  if (it == d->m_simulation.m_nodes.end())
    throw VirtualNodeNotFound(nodeId);

  d->m_queue.push_back(boost::bind(fun, it->second));
}

void SimulationSection::execute(SectionFunction fun)
{
  RecursiveUniqueLock lock(d->m_simulation.m_mutex);
  d->m_queue.push_back(fun);
}

void SimulationSection::run()
{
  RecursiveUniqueLock lock(d->m_simulation.m_mutex);
  if (d->m_simulation.m_state != Simulation::State::Running)
    return;

  TestCase::Identifier id;
  if (d->m_testCase)
    id = d->m_testCase->getId();
  else
    id = 0;

  SimulationWorkerThread &worker = d->m_simulation.m_sectionThreads[id];
  if (!worker.isRunning()) {
    if (d->m_testCase) {
      SimulationPrivate &simulation = d->m_simulation;
      d->m_testCase->signalFinished.connect([&simulation, id]() {
        RecursiveUniqueLock lock(simulation.m_mutex);
        auto it = simulation.m_sectionThreads.find(id);
        if (it == simulation.m_sectionThreads.end())
          return;

        (*it).second.stop();
        simulation.m_sectionThreads.erase(it);
      });
    }

    worker.start();
  }

  worker.push(shared_from_this());
}

void SimulationSection::schedule(int timeout)
{
  d->m_simulation.m_context.schedule(timeout, boost::bind(&SimulationSection::run, shared_from_this()));
}

SimulationPrivate::SimulationPrivate(std::uint32_t seed, size_t threads, size_t globalNodeCount)
  : m_sizeEstimator(globalNodeCount),
    m_seed(seed),
    m_threads(threads)
{
}

Simulation::Simulation(std::uint32_t seed, size_t threads, size_t globalNodeCount)
  : d(new SimulationPrivate(seed, threads, globalNodeCount))
{
}

SimulationSectionPtr Simulation::section(TestCasePtr testCase)
{
  // Can't use make_shared because the constructor is private
  return SimulationSectionPtr(new SimulationSection(*this, testCase));
}

SimulationSectionPtr Simulation::section()
{
  return section(TestCasePtr());
}

void Simulation::createNode(const std::string &name,
                            const Contact &contact,
                            const PrivatePeerKey &key,
                            const std::list<PeerPtr> &peers)
{
  RecursiveUniqueLock lock(d->m_mutex);
  VirtualNodePtr node = boost::make_shared<VirtualNode>(d->m_context, d->m_sizeEstimator, name, contact, key);
  for (PeerPtr peer : peers) {
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

std::uint32_t Simulation::seed() const
{
  return d->m_seed;
}

const Context &Simulation::context() const
{
  return d->m_context;
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
