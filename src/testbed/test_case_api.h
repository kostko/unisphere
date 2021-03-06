/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_TESTBED_TESTCASEAPI_H
#define UNISPHERE_TESTBED_TESTCASEAPI_H

#include "core/globals.h"
#include "testbed/dataset/dataset.h"
#include "testbed/exceptions.h"
#include "testbed/cluster/partition.h"
#include "testbed/test_case_fwd.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/range/adaptors.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Public interface that can be used by test cases to perform tasks.
 */
class UNISPHERE_EXPORT TestCaseApi : public boost::enable_shared_from_this<TestCaseApi> {
  friend class TestCase;
public:
  /**
   * Immediately finish the current test case. This method is only available
   * on slaves.
   */
  virtual void finishNow()
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a specific dataset.
   *
   * @param name Unique dataset name within the test case
   */
  virtual DataSet dataset(const std::string &name)
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a specific dataset belonging to another testcase.
   *
   * @param testCase Instance of another testcase
   * @param name Unique dataset name within the test case
   */
  virtual DataSet dataset(TestCasePtr testCase, const std::string &name)
  {
    throw IllegalApiCall();
  }

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
                                        const std::string &marker = "")
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a vector of node partitions. This method is only available on
   * the controller.
   */
  virtual PartitionRange getPartitions()
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a list of nodes. This method is only available on the controller.
   */
  virtual Partition::NodeRange getNodes() const
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a node descriptor for the given node. This method is only available
   * on the controller.
   *
   * @param nodeId Node identifier
   */
  virtual const Partition::Node &getNodeById(const NodeIdentifier &nodeId)
  {
    throw IllegalApiCall();
  }

  /**
   * Returns a random number generator.
   */
  virtual std::mt19937 &rng() = 0;

  /**
   * Defers function execution to simulation loop. This method is only
   * available on slaves.
   *
   * @param fun Function to defer
   * @param timeout Number of seconds to wait before running
   */
  virtual void defer(std::function<void()> fun, int timeout = 0)
  {
    throw IllegalApiCall();
  }

  /**
   * Calls a dependent test case. Its results will be available in the
   * processGlobalResults method. This method is only available on the
   * controller.
   *
   * WARNING: Using this method introduces dependencies between tests
   * and may cause loops if not careful.
   *
   * @param name Test case name
   * @return Running test case instance or null
   */
  virtual TestCasePtr callTestCase(const std::string &name)
  {
    throw IllegalApiCall();
  }

  /**
   * Sets global test case arguments that will be available in each
   * partition. This method is only available on the controller.
   *
   * @param args Global arguments
   */
  virtual void setGlobalArguments(const boost::property_tree::ptree &args)
  {
    throw IllegalApiCall();
  }

  /**
   * Returns the current timestamp in UNISPHERE epoch time. This method
   * is only available on slaves.
   */
  virtual std::uint32_t getTime()
  {
    throw IllegalApiCall();
  }
private:
  /**
   * Removes the running test case. This method is only available on the
   * controller.
   */
  virtual void removeRunningTestCase()
  {
    throw IllegalApiCall();
  }
};

UNISPHERE_SHARED_POINTER(TestCaseApi)

}

}

#endif
