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
#ifndef UNISPHERE_TESTBED_DATASET_PROCESSOR_H
#define UNISPHERE_TESTBED_DATASET_PROCESSOR_H

#include "testbed/dataset/storage.h"

#include <mongo/client/dbclient.h>

namespace UniSphere {

namespace TestBed {

/**
 * Dataset processor thread.
 */
class UNISPHERE_EXPORT DataSetProcessor {
public:
  /**
   * Class constructor.
   */
  explicit DataSetProcessor(DataSetStorage &dss);

  DataSetProcessor(const DataSetProcessor&) = delete;
  DataSetProcessor &operator=(const DataSetProcessor&) = delete;

  /**
   * Queues a BSON object for insertion.
   *
   * @param ns Namespace to insert into
   * @param bson BSON object to insert
   */
  void insert(const std::string &ns, const mongo::BSONObj &bson);

  /**
   * Waits until all objects under the specified namespace have been
   * committed into the dataset storage.
   *
   * @param ns Namespace to wait for
   */
  void wait(const std::string &ns);

  /**
   * Initializes the dataset processor.
   */
  void initialize();
private:
  UNISPHERE_DECLARE_PRIVATE(DataSetProcessor)
};

}

}

#endif
