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
#ifndef UNISPHERE_TESTBED_SCENARIOAPI_H
#define UNISPHERE_TESTBED_SCENARIOAPI_H

#include "core/globals.h"
#include "testbed/test_case_fwd.h"

namespace UniSphere {

namespace TestBed {

/**
 * Public interface that can be used by scenarios to perform tasks.
 */
class UNISPHERE_EXPORT ScenarioApi {
public:
  /**
   * Runs a specific test case immediately.
   *
   * @param name Test case to run
   * @return Test case instance
   */
  virtual TestCasePtr runTestCase(const std::string &name) = 0;

  /**
   * Runs a specific test case immediately.
   *
   * @param name Test case to run
   * @return Test case instance
   */
  virtual TestCasePtr runTestCase(const std::string &name,
                                  std::function<void()> completion) = 0;

  /**
   * Schedules a specific test case to be run after some delay.
   *
   * @param timeout Number of seconds to wait before running
   * @param name Test case to run
   */
  virtual void runTestCaseAt(int timeout, const std::string &name) = 0;

  /**
   * Schedules a specific test case to be run after some delay.
   *
   * @param timeout Number of seconds to wait before running
   * @param name Test case to run
   * @param completion Completion handler
   */
  virtual void runTestCaseAt(int timeout,
                             const std::string &name,
                             std::function<void()> completion) = 0;
};

}

}

#endif
