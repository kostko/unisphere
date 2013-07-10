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
#ifndef UNISPHERE_TESTBED_TESTCASEAPI_H
#define UNISPHERE_TESTBED_TESTCASEAPI_H

#include "core/globals.h"
#include "testbed/dataset.hpp"
#include "testbed/exceptions.h"
#include "testbed/cluster/partition.h"

namespace UniSphere {

namespace TestBed {

class TestCase;
UNISPHERE_SHARED_POINTER(TestCase)

/**
 * Public interface that can be used by test cases to perform tasks.
 */
class UNISPHERE_EXPORT TestCaseApi {
public:
  /**
   * Immediately finish the current test case. This method is only available
   * on slaves.
   */
  virtual void finishNow() = 0;

  /**
   * Transmits the specified dataset back to the controller. This method is
   * only available on slaves.
   *
   * @param dataset Dataset to transmit
   */
  template <typename DataSetType>
  void send(const DataSetType &dataset)
  {
    std::ostringstream buffer;
    boost::archive::text_oarchive archive(buffer);
    archive << dataset;
    send_(dataset.getName(), buffer.str());
  }

  /**
   * Receives an aggregated dataset from slaves. This method is only available
   * on controller.
   *
   * @param dataset Dataset to receive
   * @return True if dataset has been received, false otherwise
   */
  template <typename DataSetType>
  bool receive(DataSetType &dataset)
  {
    try {
      DataSetBuffer &buf = receive_(dataset.getName());
      dataset.clear();
      for (std::string &ds_buffer : buf) {
        DataSetType received;
        std::istringstream tmp(ds_buffer);
        boost::archive::text_iarchive archive(tmp);
        archive >> received;
        dataset.moveFrom(received);
      }
      return true;
    } catch (DataSetNotFound &e) {
      return false;
    }
  }

  /**
   * Returns a filename appropriate for output.
   *
   * @param prefix Filename prefix
   * @param extension Filename extension
   * @return A filename ready for output or an empty string if none is available
   */
  virtual std::string getOutputFilename(const std::string &prefix,
                                        const std::string &extension) = 0;

  /**
   * Returns a vector of node partitions. This method is only available on
   * the controller.
   */
  virtual const std::vector<Partition> &getPartitions() = 0;

  /**
   * Returns a random number generator.
   */
  virtual std::mt19937 &rng() = 0;

  /**
   * Defers function execution to simulation loop. This method is only
   * available on slaves.
   *
   * @param fun Function to defer
   */
  virtual void defer(std::function<void()> fun) = 0;
private:
  /**
   * Transmits the specified dataset back to the controller.
   *
   * @param dsName Data set name
   * @param dsData Data set data
   */
  virtual void send_(const std::string &dsName,
                     const std::string &dsData) = 0;

  /**
   * Receives an aggregated dataset from slaves.
   *
   * @param dsName Data set name
   * @return Reference to received data set buffer
   */
  virtual DataSetBuffer &receive_(const std::string &dsName) = 0;
};

}

}

#endif
