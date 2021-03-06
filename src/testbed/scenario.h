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
#ifndef UNISPHERE_TESTBED_SCENARIO_H
#define UNISPHERE_TESTBED_SCENARIO_H

#include "core/globals.h"
#include "core/program_options.h"
#include "testbed/scenario_api.h"

#include <boost/signals2/signal.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * A scenario defines the temporal order and type of tests
 * that will be executed.
 */
class UNISPHERE_EXPORT Scenario : public OptionModule {
public:
  /**
   * Class constructor.
   */
  Scenario(const std::string &name);

  Scenario(const Scenario&) = delete;
  Scenario &operator=(const Scenario&) = delete;

  /**
   * Starts the scenario.
   *
   * @param api API for executing this scenario
   */
  void start(ScenarioApi &api);

  /**
   * Returns the scenario name.
   */
  // TODO: Make this consistent with TestCase -> getName()
  std::string name() const;

  /**
   * Suspends execution of the scenario coroutine. This method may only
   * be called from inside the coroutine or it will throw.
   */
  void suspend();

  /**
   * Schedules the coroutine to be resumed in the scenario thread.
   */
  void resume();
public:
  /// Signal that gets emitted when scenario completes
  boost::signals2::signal<void()> signalFinished;
protected:
  /**
   * Runs the scenario.
   *
   * @param options Program options
   */
  virtual void run(ScenarioApi &api,
                   boost::program_options::variables_map &options) = 0;

  /**
   * Called by OptionModule to configure the scenario.
   */
  void setupOptions(int argc,
                    char **argv,
                    boost::program_options::options_description &options,
                    boost::program_options::variables_map &variables);

  /**
   * A simplified version of setupOptions to be used when declaring scenarios.
   */
  virtual void setupOptions(boost::program_options::options_description &options,
                            boost::program_options::variables_map &variables) {};
private:
  UNISPHERE_DECLARE_PRIVATE(Scenario)
};

UNISPHERE_SHARED_POINTER(Scenario)

}

}

#define UNISPHERE_SCENARIO(Class) struct Class : public UniSphere::TestBed::Scenario { \
                                    Class() : UniSphere::TestBed::Scenario(#Class) {}; \
                                    protected: \
                                    void run(UniSphere::TestBed::ScenarioApi &api, \
                                             boost::program_options::variables_map &options)

#define UNISPHERE_SCENARIO_END };

#define UNISPHERE_SCENARIO_END_REGISTER(Class) }; \
                                               UNISPHERE_REGISTER_SCENARIO(Class)

#endif
