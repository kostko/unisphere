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

#include <list>

namespace UniSphere {

namespace TestBed {

/**
 * Public interface that can be used by scenarios to perform tasks.
 */
class UNISPHERE_EXPORT ScenarioApi {
public:
  /**
   * Suspends scenario for a specific number of seconds.
   *
   * @param timeout Number of seconds to suspend for
   */
  virtual void wait(int timeout) = 0;

  /**
   * Runs a specific test case and waits for its completion.
   *
   * @param name Test case name
   * @return Test case instance
   */
  virtual TestCasePtr test(const std::string &name) = 0;

  /**
   * Runs multiple tests in parallel and waits for all of them to
   * complete.
   *
   * @param names A list of test case names
   * @return A list of test case instances
   */
  virtual std::list<TestCasePtr> test(std::initializer_list<std::string> names) = 0;
};

}

}

#endif
