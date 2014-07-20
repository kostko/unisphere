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
#ifndef UNISPHERE_TESTBED_DATASET_STORAGE_H
#define UNISPHERE_TESTBED_DATASET_STORAGE_H

#include "core/globals.h"

#include <mongo/client/dbclient.h>

namespace UniSphere {

namespace TestBed {

class DataSetProcessor;

/**
 * Dataset storage configuration.
 */
class UNISPHERE_EXPORT DataSetStorage {
public:
  /// Dataset storage namespace (MongoDB database name)
  static const std::string Namespace;

  /**
   * Class constructor.
   */
  DataSetStorage();

  DataSetStorage(const DataSetStorage&) = delete;
  DataSetStorage &operator=(const DataSetStorage&) = delete;

  /**
   * Configures the dataset storage connection string.
   *
   * @param connection Connection string for the storage server
   */
  void setConnectionString(const std::string &cs);

  /**
   * Returns the configured connection string.
   */
  mongo::ConnectionString getConnectionString() const;

  /**
   * Returns a reference to the dataset processor.
   */
  DataSetProcessor &getProcessor();

  /**
   * Performs dataset storage initialization.
   */
  void initialize();

  /**
   * Clears the data storage, erasing all existing data.
   */
  void clear();
private:
  UNISPHERE_DECLARE_PRIVATE(DataSetStorage)
};

}

}

#endif
