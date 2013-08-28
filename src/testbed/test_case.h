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
#include <boost/any.hpp>

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
   * Test case argument.
   */
  struct Argument {
    /// Argument name
    std::string name;
    /// Argument value
    boost::any value;
  };
  /// Argument initializer list type
  typedef std::initializer_list<Argument> ArgumentList;

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
   * Configures test case arguments. These arguments are only visible on
   * the controller -- in order to transfer any of them to slave instances,
   * they must be forwarded in selectNode for each virtual node.
   *
   * @param arguments A list of arguments
   */
  void setArguments(ArgumentList arguments);

  /**
   * Returns true if the test case has finished.
   */
  bool isFinished() const;

  /**
   * Try to complete the global test case instance. This method is
   * called by the controller.
   *
   * @param api Test case API interface
   */
  void tryComplete(TestCaseApi &api);

  /**
   * Adds a child test case. The controller will wait on all children
   * to complete before completing the parent test case.
   *
   * @param child Child test case
   */
  void addChild(TestCasePtr child);

  /**
   * Runs before any node selection is performed on the controller. May
   * be used to schedule additional sub-testcases to be run.
   *
   * Default implementation does nothing.
   *
   * @param api Test case API interface
   */
  virtual void preSelection(TestCaseApi &api);

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
   * @param api Test case API interface
   * @return Selected node descriptor
   */
  virtual SelectedPartition::Node selectNode(const Partition &partition,
                                             const Partition::Node &node,
                                             TestCaseApi &api);

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
   * This method is called on the slaves when a signal has been
   * received from the test controller.
   *
   * Default implementation does nothing.
   *
   * @param api Test case API interface
   * @param signal Signal name
   */
  virtual void signalReceived(TestCaseApi &api,
                              const std::string &signal);

  /**
   * This method is run on the slaves after runNode has been called
   * for all virtual nodes in the local partition.
   *
   * Default implementation does nothing.
   *
   * @param api Test case API interface
   */
  virtual void localNodesRunning(TestCaseApi &api);

  /**
   * This method is run on the slaves after runNode has been called
   * for all virtual nodes in the local partition and the test case has
   * finished. It can be used to perform further processing of local results.
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
public:
  /// This signal gets emitted after a test case finishes on the controller
  boost::signals2::signal<void()> signalFinished;
protected:
  /**
   * Marks the test case as a candidate for finishing.
   */
  void finish(TestCaseApi &api);

  /**
   * Logger that can be used for reporting from test cases.
   */
  Logger &logger();

  /**
   * Returns the specified test case argument. This method is only available
   * on the controller -- on slaves, there will be no test-case arguments.
   *
   * @param name Argument name
   * @return Argument value or an invalid value when the argument is not set
   */
  boost::any argument(const std::string &name) const;

  /**
   * Returns the specified test case argument. This method is only available
   * on the controller -- on slaves, there will be no test-case arguments.
   *
   * @param name Argument name
   * @param def Optional default value to return when argument doesn't exist
   * @return Argument value 
   */
  template <typename T>
  T argument(const std::string &name, const T &def = T()) const
  {
    boost::any arg = argument(name);
    if (arg.empty())
      return def;

    try {
      return boost::any_cast<T>(arg);
    } catch (const boost::bad_any_cast&) {
      // Automatically convert const char* to std::string
      if (typeid(T) == typeid(std::string))
        return std::string(boost::any_cast<const char*>(arg));
      else
        throw;
    }
  }
private:
  UNISPHERE_DECLARE_PRIVATE(TestCase)
};

UNISPHERE_SHARED_POINTER(TestCase)

}

}

#endif
