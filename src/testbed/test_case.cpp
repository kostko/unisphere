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

#include <boost/log/attributes/constant.hpp>
#include <botan/botan.h>

namespace UniSphere {

namespace TestBed {

class TestCasePrivate {
public:
  TestCasePrivate(const std::string &name);
public:
  /// Unique test case identifier
  TestCase::Identifier m_id;
  /// Test case name
  std::string m_name;
  /// Virtual node map
  VirtualNodeMap *m_nodes;
  /// Logger instance
  Logger m_logger;
};

TestCasePrivate::TestCasePrivate(const std::string &name)
  : m_name(name),
    m_nodes(nullptr),
    m_logger(logging::keywords::channel = "test_case")
{
  // Generate random test case identifier
  Botan::AutoSeeded_RNG rng;
  rng.randomize((Botan::byte*) &m_id, sizeof(m_id));

  m_logger.add_attribute("TestCase", logging::attributes::constant<std::string>(name));
}

TestCase::TestCase(const std::string &name)
  : d(new TestCasePrivate(name)),
    testbed(TestBed::getGlobalTestbed())
{
}

TestCase::Identifier TestCase::getId() const
{
  return d->m_id;
}

SelectedPartition::Node TestCase::selectNode(const Partition &partition,
                                             const Partition::Node &node) const
{
  // By default we select all nodes and pass empty arguments to test case run
  return SelectedPartition::Node{ node.contact.nodeId() };
}

void TestCase::run()
{
  /*if (snapshot()) {
    testbed.snapshot(boost::bind(&TestCase::start, this));
  } else {
    start();
  }*/
}

void TestCase::finish()
{
  signalFinished();
  //testbed.finishTestCase(shared_from_this());
}

bool TestCase::snapshot()
{
  return false;
}

int TestCase::time() const
{
  //return testbed.time();
  return 0;
}

VirtualNodeMap &TestCase::nodes()
{
  return *d->m_nodes;
}

Logger &TestCase::logger()
{
  return d->m_logger;
}

DataCollector TestCase::data(const std::string &category,
                             std::initializer_list<std::string> columns,
                             const std::string &type)
{
  std::string component = d->m_name;
  if (!category.empty())
    component += "-" + category;

  return DataCollector(
    ".", // XXX
    component,
    columns,
    type
  );
}

DataCollector TestCase::data(const std::string &category,
                             const std::string &type)
{
  return data(category, {}, type);
}

void TestCase::require(bool assertion)
{
  if (!assertion) {
    BOOST_LOG_SEV(d->m_logger, log::error) << "Requirement not satisfied.";
  }
}

}

}
