/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "testbed/test_case.h"
#include "testbed/cluster/partition.h"

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
  template <typename T = TestCase>
  boost::shared_ptr<T> test(const std::string &name,
                            typename TestCase::ArgumentList args = TestCase::ArgumentList())
  {
    return boost::static_pointer_cast<T>(test_(name, args));
  }

  /**
   * Runs multiple tests in parallel and waits for all of them to
   * complete.
   *
   * @param names A list of test case names
   * @return A list of test case instances
   */
  virtual std::list<TestCasePtr> test(std::initializer_list<std::string> names) = 0;

  /**
   * Runs a specific test case and doesn't wait for its completion.
   *
   * @param name  Test case name
   * @return Test case instance
   */
  template <typename T = TestCase>
  boost::shared_ptr<T> testInBackground(const std::string &name,
                                        typename TestCase::ArgumentList args = TestCase::ArgumentList())
  {
    return boost::static_pointer_cast<T>(testInBackground_(name, args));
  }

  /**
   * Signal a running test case and wait for test case completion.
   *
   * @param test Test case instance
   * @param signal Signal to send the test case
   */
  virtual void signal(TestCasePtr test,
                      const std::string &signal) = 0;

  /**
   * Returns a vector of node partitions.
   */
  virtual PartitionRange getPartitions() const = 0;

  /**
   * Returns a list of nodes.
   */
  virtual Partition::NodeRange getNodes() const = 0;

  /**
   * Requests specific nodes to start.
   *
   * @param nodes A list of nodes   */
  virtual void startNodes(const Partition::NodeRange &nodes) = 0;

  /**
   * Request specific nodes to start in batches of specific size and
   * with specified delay between batches.
   *
   * @param nodes A list of nodes
   * @param batchSize Maximum size of each batch
   * @param delay Delay between batches (in seconds)
   */
  void startNodesBatch(const Partition::NodeRange &nodes,
                       size_t batchSize,
                       time_t delay)
  {
    if (!batchSize)
      return;

    auto start = nodes.begin();
    auto end = nodes.begin();
    size_t count = 0;
    for (;;) {
      count++;

      if (++end == nodes.end() || count == batchSize) {
        startNodes(Partition::NodeRange(start, end));
        wait(delay);

        if (end == nodes.end())
          break;

        start = end;
        count = 0;
      }
    }
  }

  /**
   * Requests to start a specific node.
   *
   * @param nodeId Node identifier
   */
  virtual void startNode(const NodeIdentifier &nodeId) = 0;

  /**
   * Requests to stop a specific node.
   *
   * @param nodeId Node identifier
   */
  virtual void stopNode(const NodeIdentifier &nodeId) = 0;

  /**
   * Returns a filename appropriate for output.
   *
   * @param prefix Filename prefix
   * @param extension Filename extension
   * @param marker Optional marker
   * @return A filename ready for output or an empty string if none is available
   */
  virtual std::string getOutputFilename(const std::string &prefix,
                                        const std::string &extension,
                                        const std::string &marker = "") = 0;
protected:
  /**
   * Runs a specific test case and waits for its completion.
   *
   * @param name Test case name
   * @return Test case instance
   */
  virtual TestCasePtr test_(const std::string &name,
                            typename TestCase::ArgumentList args = TestCase::ArgumentList()) = 0;

  /**
   * Runs a specific test case and doesn't wait for its completion.
   *
   * @param name  Test case name
   * @return Test case instance
   */
  virtual TestCasePtr testInBackground_(const std::string &name,
                                        typename TestCase::ArgumentList args = TestCase::ArgumentList()) = 0;
};

}

}

#endif
