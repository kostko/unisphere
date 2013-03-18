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
#ifndef UNISPHERE_TESTBED_TESTCASE_H
#define UNISPHERE_TESTBED_TESTCASE_H

#include <ostream>
#include <string>

#include <boost/enable_shared_from_this.hpp>

#include "core/globals.h"
#include "testbed/nodes.h"

namespace UniSphere {

namespace TestBed {

class TestBed;

/**
 * A test case that can be executed.
 */
class UNISPHERE_EXPORT TestCase : public boost::enable_shared_from_this<TestCase> {
public:
  /**
   * Class constructor.
   */
  TestCase();

  TestCase(const TestCase&) = delete;
  TestCase &operator=(const TestCase&) = delete;

  /**
   * Runs the test case.
   */
  void run();
protected:
  /**
   * This method should provide the code that will execute the actual
   * test case.
   */
  virtual void start() = 0;

  /**
   * Should return true if this test case should be run inside a
   * snapshot.
   */
  virtual bool snapshot();

  /**
   * Returns the current time since testbed start.
   */
  int time() const;

  /**
   * Requires a given assertion to be true.
   */
  void require(bool assertion);

  /**
   * Reporting stream.
   */
  std::ostream &report();

  /**
   * Returns the virtual node map instance.
   */
  VirtualNodeMap &nodes();

  /**
   * Returns the node name map instance.
   */
  NodeNameMap &names();

  /**
   * Notifies the testbed that this test case is finished.
   */
  void finish();
private:
  friend class TestBedPrivate;

  /**
   * Initializes the test case.
   *
   * @param name Test case name
   * @param nodes Virtual node map
   * @param names Node name map
   */
  void initialize(const std::string &name, VirtualNodeMap *nodes, NodeNameMap *names);
private:
  UNISPHERE_DECLARE_PRIVATE(TestCase)
};

UNISPHERE_SHARED_POINTER(TestCase)

}

}

#endif
