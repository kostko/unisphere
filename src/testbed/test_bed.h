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
#include "testbed/simulation.h"

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
   * Returns a map of registered scenarios.
   */
  const std::map<std::string, ScenarioPtr> &scenarios() const;

  /**
   * Returns a scenario with specific id.
   *
   * @param id Scenario identifier
   * @return A corresponding scenario instance or null when one doesn't exist
   */
  ScenarioPtr getScenario(const std::string &id) const;

  /**
   * Creates a new instance of the specified test case.
   *
   * @param id Test case identifier
   * @return A corresponding test case or null when one doesn't exist
   */
  TestCasePtr createTestCase(const std::string &id) const;

  /**
   * Creates a new simulation.
   *
   * @param seed Seed for the simulation's basic RNG
   * @param threads Number of threads
   * @param globalNodeCount Number of all nodes (over the whole cluster)
   * @return A new simulation instance
   */
  SimulationPtr createSimulation(std::uint32_t seed,
                                 size_t threads,
                                 size_t globalNodeCount);

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
   * Factory constructor.
   *
   * @param name Test case name
   */
  TestCaseFactory(const std::string &name)
    : m_name(name)
  {}

  /**
   * Creates a new test case instance and returns it.
   */
  virtual TestCasePtr create() = 0;
protected:
  /// Test case name
  std::string m_name;
};

UNISPHERE_SHARED_POINTER(TestCaseFactory)

/**
 * Generic factory for creating test case instances of the specified
 * class.
 */
template <typename T>
class UNISPHERE_EXPORT GenericTestCaseFactory : public TestCaseFactory {
public:
  using TestCaseFactory::TestCaseFactory;

  /**
   * Creates a new test case instance and returns it.
   */
  TestCasePtr create() { return TestCasePtr(new T(m_name)); }
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
    TestBed::getGlobalTestbed().registerTestCase(name, new GenericTestCaseFactory<T>(name));
  }
};

/**
 * Automatic scenario registration class.
 */
template <typename T>
class UNISPHERE_EXPORT RegisterScenario {
public:
  /**
   * Registers the given scenario.
   */
  RegisterScenario()
  {
    TestBed::getGlobalTestbed().registerScenario(new T);
  }
};

}

}

/// A macro for easier test case registration
#define UNISPHERE_REGISTER_TEST_CASE(Class, name) namespace { UniSphere::TestBed::RegisterTestCase<Class> unisphere_testcase##Class(name); }
/// A macro for easier scenario registration
#define UNISPHERE_REGISTER_SCENARIO(Class) namespace { UniSphere::TestBed::RegisterScenario<Class> unisphere_scenario##Class; }

#endif
