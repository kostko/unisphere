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
#ifndef UNISPHERE_TESTBED_SCENARIO_H
#define UNISPHERE_TESTBED_SCENARIO_H

#include "core/globals.h"

namespace UniSphere {

namespace TestBed {

class TestBed;

/**
 * A scenario defines the temporal order and type of tests
 * that will be executed.
 */
class UNISPHERE_EXPORT Scenario {
public:
  /**
   * Class constructor.
   */
  Scenario();

  Scenario(const Scenario&) = delete;
  Scenario &operator=(const Scenario&) = delete;

  /**
   * Performs scenario setup.
   */
  virtual void setup() = 0;
protected:
  /// Testbed instance
  TestBed &testbed;
};

UNISPHERE_SHARED_POINTER(Scenario)

}

}

#endif
