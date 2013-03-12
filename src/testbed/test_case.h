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

class UNISPHERE_EXPORT TestCase : public boost::enable_shared_from_this<TestCase> {
public:
  TestCase();

  TestCase(const TestCase&) = delete;
  TestCase &operator=(const TestCase&) = delete;

  void run();
protected:
  virtual void start() = 0;

  void require(bool assertion);

  std::ostream &report();

  VirtualNodeMap &nodes() { return *m_nodes; }

  NodeNameMap &names() { return *m_names; }

  void finish();
private:
  friend class TestBedPrivate;

  void initialize(TestBed *testbed, VirtualNodeMap *nodes, NodeNameMap *names);
private:
  std::string m_name;
  std::ostream &m_output;
  TestBed *m_testbed;
  VirtualNodeMap *m_nodes;
  NodeNameMap *m_names;
};

UNISPHERE_SHARED_POINTER(TestCase)

}

}

#endif
