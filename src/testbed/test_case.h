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
#include <boost/signals2/signal.hpp>

#include "core/globals.h"
#include "testbed/nodes.h"
#include "testbed/cluster/partition.h"
#include "testbed/test_case_api.h"

namespace UniSphere {

namespace TestBed {

class TestBed;

/**
 * A test case that can be executed by the distributed test bed.
 */
class UNISPHERE_EXPORT TestCase : public boost::enable_shared_from_this<TestCase> {
public:
  /// Test case identifier type
  typedef std::uint32_t Identifier;
public:
  /**
   * Current state the test case instance is in.
   */
  enum class State {
    /// Test case is in the process of being initialized (runNode metod is being
    /// called for each virtual node)
    Initializing,
    /// Test case is running in the background (runNode has completed, but has not
    /// marked the test case as being finished -- some other handler must do so)
    Running,
    /// Test case has finished
    Finished
  };

  /**
   * Class constructor.
   */
  TestCase(const std::string &name);

  TestCase(const TestCase&) = delete;
  TestCase &operator=(const TestCase&) = delete;

  /**
   * Returns the test case name.
   */
  std::string &getName() const;

  /**
   * Returns the test case's unique identifier.
   */
  Identifier getId() const;

  /**
   * Sets the test case's unique identifier.
   *
   * @param id Identifier
   */
  void setId(Identifier id);

  /**
   * Changes current test case state.
   *
   * @param state New state
   */
  void setState(State state);

  /**
   * Returns true if the test case has finished.
   */
  bool isFinished() const;

  /**
   * Selects which nodes will execute this test case. This method is
   * called by the controller for each virtual node and should return a
   * valid selected node descriptor when a node is to be included.
   * Returning an invalid (default-constructed) descriptor will cause
   * the node to be excluded.
   *
   * Default implementation selects all nodes and passes empty arguments
   * for each node.
   *
   * @param partition Node's assigned partition
   * @param node Virtual node descriptor
   * @return Selected node descriptor
   */
  virtual SelectedPartition::Node selectNode(const Partition &partition,
                                             const Partition::Node &node);

  /**
   * This method is run on the slaves for each node that has been
   * previously selected. It should setup the test case.
   *
   * Default implementation marks the test case as finished.
   *
   * @param api Test case API interface
   * @param node Virtual node instance
   * @param args Arguments passed from controller
   */
  virtual void runNode(TestCaseApi &api,
                       VirtualNodePtr node,
                       const boost::property_tree::ptree &args);

  /**
   * This method is run on the slaves after runNode has been called
   * for all virtual nodes in the local partition. It can be used to
   * perform further processing of local results.
   *
   * Default implementation does nothing.
   *
   * @param api Test case API interface
   */
  virtual void processLocalResults(TestCaseApi &api);

  /**
   * This method is run on the controller after test cases in all
   * partitions have been completed and all results received. It can
   * be used to perform processing and reporting of overall test case
   * results.
   *
   * Default implementation does nothing.
   *
   * @param api Test case API interface
   */
  virtual void processGlobalResults(TestCaseApi &api);
protected:
  /**
   * Marks the test case as a candidate for finishing.
   */
  void finish(TestCaseApi &api);

  /**
   * Logger that can be used for reporting from test cases.
   */
  Logger &logger();
private:
  UNISPHERE_DECLARE_PRIVATE(TestCase)
};

UNISPHERE_SHARED_POINTER(TestCase)

}

}

#endif
