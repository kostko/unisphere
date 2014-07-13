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
#include "testbed/test_bed.h"
#include "testbed/test_case.h"
#include "testbed/scenario.h"

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

class TestBedPrivate {
public:
  TestBedPrivate();
public:
  /// Global instance of the testbed
  static TestBed *m_self;
  /// Mutex to protect the data structure
  std::recursive_mutex m_mutex;
  /// Logger instance
  Logger m_logger;
  /// Registered test case factories
  std::map<std::string, TestCaseFactoryPtr> m_testCases;
  /// Registered scenarios
  std::map<std::string, ScenarioPtr> m_scenarios;
  /// Dataset storage
  DataSetStorage m_datasetStorage;
};

/// Global instance of the testbed
TestBed *TestBedPrivate::m_self = nullptr;

TestBedPrivate::TestBedPrivate()
  : m_logger(logging::keywords::channel = "testbed")
{
}

TestBed::TestBed()
  : d(new TestBedPrivate)
{
}

TestBed &TestBed::getGlobalTestbed()
{
  if (!TestBedPrivate::m_self)
    TestBedPrivate::m_self = new TestBed();

  return *TestBedPrivate::m_self;
}

const std::map<std::string, ScenarioPtr> &TestBed::scenarios() const
{
  return d->m_scenarios;
}

ScenarioPtr TestBed::getScenario(const std::string &id) const
{
  RecursiveUniqueLock lock(d->m_mutex);
  auto it = d->m_scenarios.find(id);
  if (it == d->m_scenarios.end())
    return ScenarioPtr();

  return it->second;
}

TestCasePtr TestBed::createTestCase(const std::string &id,
                                    typename TestCase::ArgumentList args) const
{
  RecursiveUniqueLock lock(d->m_mutex);
  auto it = d->m_testCases.find(id);
  if (it == d->m_testCases.end())
    return TestCasePtr();

  return it->second->create(args);
}

TestCasePtr TestBed::createTestCase(const std::string &id) const
{
  return createTestCase(id, TestCase::ArgumentList());
}

SimulationPtr TestBed::createSimulation(std::uint32_t seed,
                                        size_t threads,
                                        size_t globalNodeCount)
{
  SimulationPtr simulation = boost::make_shared<Simulation>(seed, threads, globalNodeCount);
  return simulation;
}

void TestBed::registerTestCase(const std::string &name, TestCaseFactory *factory)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_testCases.insert({{ name, TestCaseFactoryPtr(factory) }});
}

void TestBed::registerScenario(Scenario *scenario)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_scenarios.insert({{ scenario->name(), ScenarioPtr(scenario) }});
}

DataSetStorage &TestBed::getDataSetStorage()
{
  return d->m_datasetStorage;
}

}

}