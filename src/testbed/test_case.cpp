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

#include <set>
#include <boost/log/attributes/constant.hpp>
#include <botan/botan.h>

namespace UniSphere {

namespace TestBed {

class TestCasePrivate {
public:
  TestCasePrivate(const std::string &name);
public:
  /// Mutex
  std::recursive_mutex m_mutex;
  /// Unique test case identifier
  TestCase::Identifier m_id;
  /// Test case name
  std::string m_name;
  /// Virtual node map
  VirtualNodeMap *m_nodes;
  /// Logger instance
  Logger m_logger;
  /// Test case state
  TestCase::State m_state;
  /// Parent test case
  TestCaseWeakPtr m_parent;
  /// Running children test cases
  std::set<TestCasePtr> m_children;
  /// Temporary API storage until all children complete
  TestCaseApiPtr m_storedApi;
};

TestCasePrivate::TestCasePrivate(const std::string &name)
  : m_name(name),
    m_nodes(nullptr),
    m_logger(logging::keywords::channel = "test_case"),
    m_state(TestCase::State::Initializing)
{
  // Generate random test case identifier
  Botan::AutoSeeded_RNG rng;
  rng.randomize((Botan::byte*) &m_id, sizeof(m_id));

  m_logger.add_attribute("TestCase", logging::attributes::constant<std::string>(name));
}

TestCase::TestCase(const std::string &name)
  : d(new TestCasePrivate(name))
{
}

std::string &TestCase::getName() const
{
  return d->m_name;
}

TestCase::Identifier TestCase::getId() const
{
  return d->m_id;
}

void TestCase::setId(Identifier id)
{
  d->m_id = id;
}

void TestCase::setState(State state)
{
  d->m_state = state;
}

bool TestCase::isFinished() const
{
  return d->m_state == State::Finished;
}

void TestCase::addChild(TestCasePtr child)
{
  BOOST_ASSERT(child->d->m_parent.expired());

  RecursiveUniqueLock lock(d->m_mutex);
  child->d->m_parent = shared_from_this();
  d->m_children.insert(child);
}

void TestCase::tryComplete(TestCaseApi &api)
{
  TestCasePtr self = shared_from_this();
  TestCaseApiPtr papi = api.shared_from_this();
  RecursiveUniqueLock lock(d->m_mutex);

  // Mark this test case as finished and remove ourselves from running cases
  d->m_state = State::Finished;
  api.removeRunningTestCase();

  // Check if we are waiting on any child test cases to complete first
  if (!d->m_children.empty()) {
    // Store the API locally as it will be destroyed otherwise, since the case
    // is no longer running; we will also need it later when invoked by a child
    d->m_storedApi = papi;
    return;
  }

  // TODO: This could take some time, should it be dispatched into another thread?
  processGlobalResults(api);
  BOOST_LOG_SEV(d->m_logger, log::normal) << "Test case '" << getName() << "' done.";

  // We are now complete, so try to complete parent if one exists
  if (TestCasePtr parent = d->m_parent.lock()) {
    RecursiveUniqueLock plock(parent->d->m_mutex);
    parent->d->m_children.erase(shared_from_this());
    if (parent->d->m_storedApi)
      parent->tryComplete(*parent->d->m_storedApi);
  }

  // Clear our reference to stored API if one exists so it can be destroyed
  d->m_storedApi.reset();
}

void TestCase::preSelection(TestCaseApi &api)
{
}

SelectedPartition::Node TestCase::selectNode(const Partition &partition,
                                             const Partition::Node &node,
                                             TestCaseApi &api)
{
  // By default we select all nodes and pass empty arguments to test case run
  return SelectedPartition::Node{ node.contact.nodeId() };
}

void TestCase::runNode(TestCaseApi &api,
                       VirtualNodePtr node,
                       const boost::property_tree::ptree &args)
{
  finish(api);
}

void TestCase::localNodesRunning(TestCaseApi &api)
{
}

void TestCase::processLocalResults(TestCaseApi &api)
{
}

void TestCase::processGlobalResults(TestCaseApi &api)
{
}

void TestCase::finish(TestCaseApi &api)
{
  if (d->m_state == State::Running) {
    // We should immediately finish with this test case
    d->m_state = State::Finished;
    processLocalResults(api);
    api.finishNow();
  } else {
    d->m_state = State::Finished;
  }
}

Logger &TestCase::logger()
{
  return d->m_logger;
}

}

}
