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
   * Immediately finish the current test case.
   */
  virtual void finishNow() = 0;

  /**
   * Transmits the specified dataset back to the controller.
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
   * Receives an aggregated dataset from slaves.
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
