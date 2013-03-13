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

namespace UniSphere {

namespace TestBed {

TestCase::TestCase()
  : m_output(std::cout),
    m_testbed(TestBed::getGlobalTestbed()),
    m_nodes(nullptr),
    m_names(nullptr)
{
}

void TestCase::initialize(const std::string &name, VirtualNodeMap *nodes, NodeNameMap *names)
{
  m_name = name;
  m_nodes = nodes;
  m_names = names;
}

void TestCase::run()
{
  start();
}

void TestCase::finish()
{
  m_testbed.finishTestCase(shared_from_this());
}

std::ostream &TestCase::report()
{
  // TODO: Serialize access to output stream
  m_output << "[TestCase::" << m_name << "] ";
  return m_output;
}

void TestCase::require(bool assertion)
{
  if (!assertion) {
    report() << "ERROR: Requirement not satisfied.";
    // TODO
  }
}

}

}
