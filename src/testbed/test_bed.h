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
#ifndef UNISPHERE_TESTBED_TESTBED_H
#define UNISPHERE_TESTBED_TESTBED_H

#include "core/globals.h"
#include "testbed/test_case.h"
#include "testbed/scenario.h"

namespace UniSphere {

namespace TestBed {

class TestCaseFactory;

/**
 * Testbed entry point - this is a singleton class.
 */
class UNISPHERE_EXPORT TestBed {
public:
  friend class TestCase;
  
  TestBed(const TestBed&) = delete;
  TestBed &operator=(const TestBed&) = delete;

  /**
   * Returns the global testbed instance.
   */
  static TestBed &getGlobalTestbed();

  /**
   * Sets up the "physical" network that will be used by the nodes for
   * their direct communication.
   *
   * @param ip IP address nodes will bind to
   * @param port Port number of the first node
   */
  void setupPhyNetwork(const std::string &ip, unsigned short port);

  /**
   * Performs program options processing and runs the proper scenario.
   */
  int run(int argc, char **argv);

  /**
   * Registers a new test case class.
   *
   * @param name Test case name
   * @param factory Factory that produces instances of the new test case class
   */
  void registerTestCase(const std::string &name, TestCaseFactory *factory);

  /**
   * Registers the given scenario. Testbed takes ownership of the instance.
   *
   * @param scenario Scenario instance
   */
  void registerScenario(Scenario *scenario);

  /**
   * Runs the given scenario instance.
   *
   * @param scenario Scenario name
   * @return True when scenario was successfully run, false if it doesn't exist
   */
  bool runScenario(const std::string &scenario);

  /**
   * Loads the given trust network topology.
   */
  void loadTopology(const std::string &topologyFile);

  /**
   * Runs a new instance of the test case identified by its name.
   *
   * @param test Test case name
   */
  void runTest(const std::string &test);

  /**
   * Schedule a new instance of the test case to be run at a specific point
   * in time.
   *
   * @param time Number of seconds from now after which the test should run
   * @param test Test case name
   */
  void scheduleTest(int time, const std::string &test);

  /**
   * Schedules a new instance of the test case to be run at specific
   * intervals.
   *
   * @param time Time interval (in seconds)
   * @param test Test case name
   */
  void scheduleTestEvery(int time, const std::string &test);

  /**
   * Schedules a call to a custom handler.
   *
   * @param time Number of seconds from now after which the test should run
   * @param handler The handler that should b executed
   */
  void scheduleCall(int time, std::function<void()> handler);

  /**
   * Ends the scenario after this amount of time.
   */
  void endScenarioAfter(int time);

  /**
   * Performs a snapshot operation. All virtual nodes are suspended before
   * the handler is invoked and resumed after it completes. The handler
   * must complete as fast as possible.
   *
   * @param handler Snapshot handler
   */
  void snapshot(std::function<void()> handler);

  /**
   * Returns the current discrete time since simulation start.
   */
  int time() const;

  /**
   * Returns the configured output directory.
   */
  std::string getOutputDirectory() const;

  /**
   * Treat the specified test case instance as finished.
   *
   * @param test Test case instance
   */
  void finishTestCase(TestCasePtr test);
protected:
  /**
   * Class constructor.
   */
  TestBed();

  /**
   * Returns the used UNISPHERE context.
   */
  Context &getContext();
private:
  UNISPHERE_DECLARE_PRIVATE(TestBed)
};

/**
 * Factory interface for creating test case instances.
 */
class UNISPHERE_EXPORT TestCaseFactory {
public:
  /**
   * Creates a new test case instance and returns it.
   */
  virtual TestCasePtr create() = 0;
};

UNISPHERE_SHARED_POINTER(TestCaseFactory)

/**
 * Generic factory for creating test case instances of the specified
 * class.
 */
template <typename T>
class UNISPHERE_EXPORT GenericTestCaseFactory : public TestCaseFactory {
public:
  /**
   * Creates a new test case instance and returns it.
   */
  TestCasePtr create() { return TestCasePtr(new T); }
};

/**
 * Automatic test case registration class.
 */
template <typename T>
class UNISPHERE_EXPORT RegisterTestCase {
public:
  /**
   * Registers the given test case class under the specified name.
   *
   * @param name Test case name
   */
  RegisterTestCase(const std::string &name)
  {
    TestBed::getGlobalTestbed().registerTestCase(name, new GenericTestCaseFactory<T>);
  }
};

}

}

/// A macro for easier test case registration
#define UNISPHERE_REGISTER_TEST_CASE(Class, name) namespace { UniSphere::TestBed::RegisterTestCase<Class> testcase##Class(name); }

#endif
