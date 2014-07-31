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
#include "testbed/scenario.h"
#include "testbed/test_bed.h"
#include "testbed/exceptions.h"

#include <boost/thread.hpp>
#include <boost/coroutine/all.hpp>

namespace UniSphere {

namespace TestBed {

/// Scenario coroutine type (pull)
using ScenarioCoroutinePull = boost::coroutines::coroutine<void>::pull_type;
/// Scenario coroutine type (push)
using ScenarioCoroutinePush = boost::coroutines::coroutine<void>::push_type;

class ScenarioPrivate {
public:
  explicit ScenarioPrivate(const std::string &name);
public:
  /// Scenario name
  std::string m_name;
  /// Scenario configuration
  boost::program_options::variables_map m_options;
  /// Scenario thread
  boost::thread m_thread;
  /// Scenario ASIO event loop
  boost::asio::io_service m_io;
  /// Work to keep the event loop busy
  boost::asio::io_service::work m_work;
  /// Scenario coroutine (pull)
  ScenarioCoroutinePull m_coroutine;
  /// Scenario corouting (push)
  ScenarioCoroutinePush *m_coroutineCaller;
};

ScenarioPrivate::ScenarioPrivate(const std::string &name)
  : m_name(name),
    m_work(m_io)
{
}

Scenario::Scenario(const std::string &name)
  : d(new ScenarioPrivate(name))
{
}

std::string Scenario::name() const
{
  return d->m_name;
}

void Scenario::resume()
{
  if (boost::this_thread::get_id() == d->m_thread.get_id())
    throw IllegalApiCall();

  // Request the coroutine to resume execution in the scenario thread
  d->m_io.post([this]() { d->m_coroutine(); });
}

void Scenario::suspend()
{
  if (!d->m_coroutineCaller)
    throw ScenarioNotRunning();

  // Ensure that suspend can only be called from the scenario thread/coroutine
  if (boost::this_thread::get_id() != d->m_thread.get_id())
    throw IllegalApiCall();

  // Suspend current coroutine
  (*d->m_coroutineCaller)();
}

void Scenario::start(ScenarioApi &api)
{
  d->m_thread = std::move(boost::thread([this, &api]() {
    // Initialize and enter the scenario coroutine
    d->m_coroutine = std::move(ScenarioCoroutinePull([this, &api](ScenarioCoroutinePush &ca) {
      d->m_coroutineCaller = &ca;
      run(api, d->m_options);
      d->m_coroutineCaller = nullptr;
      d->m_io.stop();
    }));

    // Start ASIO event loop to allow forwarding of coroutine events
    d->m_io.run();

    // Emit finished signal
    signalFinished();
  }));
}

void Scenario::setupOptions(int argc,
                            char **argv,
                            boost::program_options::options_description &options,
                            boost::program_options::variables_map &variables)
{
  boost::program_options::options_description local("Scenario " + d->m_name);
  setupOptions(local, variables);

  if (variables.empty())
    options.add(local);
  else
    d->m_options = variables;
}

}

}
