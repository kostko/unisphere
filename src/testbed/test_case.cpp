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
#include "testbed/test_case.h"
#include "testbed/test_bed.h"
#include "core/context.h"

namespace UniSphere {

namespace TestBed {

class TestCasePrivate {
public:
  TestCasePrivate();
public:
  /// Test case name
  std::string m_name;
  /// Report output stream
  std::ostream &m_output;
  /// Test bed instance
  TestBed &m_testbed;
  /// Virtual node map
  VirtualNodeMap *m_nodes;
  /// Node name map
  NodeNameMap *m_names;
};

TestCasePrivate::TestCasePrivate()
  : m_output(std::cout),
    m_testbed(TestBed::getGlobalTestbed()),
    m_nodes(nullptr),
    m_names(nullptr)
{
}

TestCase::TestCase()
  : d(new TestCasePrivate)
{
}

void TestCase::initialize(const std::string &name, VirtualNodeMap *nodes, NodeNameMap *names)
{
  d->m_name = name;
  d->m_nodes = nodes;
  d->m_names = names;
}

void TestCase::run()
{
  start();
}

void TestCase::finish()
{
  d->m_testbed.finishTestCase(shared_from_this());
}

VirtualNodeMap &TestCase::nodes()
{
  return *d->m_nodes;
}

NodeNameMap &TestCase::names()
{
  return *d->m_names;
}

std::ostream &TestCase::report()
{
  std::ostream &os = d->m_testbed.getContext().logger().stream();
  os << Logger::Component{"TestCase::" + d->m_name};
  os << Logger::Level::Info;
  return os;
}

void TestCase::require(bool assertion)
{
  if (!assertion) {
    report() << Logger::Level::Error << "Requirement not satisfied." << std::endl;
  }
}

}

}
