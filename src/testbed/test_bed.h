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

class UNISPHERE_EXPORT TestBed {
public:
  TestBed(const TestBed&) = delete;
  TestBed &operator=(const TestBed&) = delete;

  static TestBed &getGlobalTestbed();

  void setupPhyNetwork(const std::string &ip, unsigned short port);

  void run();

  void registerTestCase(const std::string &name, TestCaseFactory *factory);

  void loadScenario(Scenario *scenario);

  void loadTopology(const std::string &topologyFile);

  void runTest(const std::string &test);

  void scheduleTest(int time, const std::string &test);

  void scheduleTestEvery(int time, const std::string &test);

  void scheduleCall(int time, std::function<void()> handler);

  void endScenarioAfter(int time);

  void finishTestCase(TestCasePtr test);
protected:
  TestBed();
private:
  UNISPHERE_DECLARE_PRIVATE(TestBed)
};

class UNISPHERE_EXPORT TestCaseFactory {
public:
  virtual TestCasePtr create() = 0;
};

UNISPHERE_SHARED_POINTER(TestCaseFactory)

template <typename T>
class UNISPHERE_EXPORT GenericTestCaseFactory : public TestCaseFactory {
public:
  TestCasePtr create() { return TestCasePtr(new T); }
};

template <typename T>
class UNISPHERE_EXPORT RegisterTestCase {
public:
  RegisterTestCase(const std::string &name)
  {
    TestBed::getGlobalTestbed().registerTestCase(name, new GenericTestCaseFactory<T>);
  }
};

}

}

#define UNISPHERE_REGISTER_TEST_CASE(Class, name) namespace { UniSphere::TestBed::RegisterTestCase<Class> testcase##Class(name); }

#endif
