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
#ifndef UNISPHERE_TESTBED_DATASET_DATASET_H
#define UNISPHERE_TESTBED_DATASET_DATASET_H

#include "core/globals.h"
#include "identity/node_identifier.h"

#include <mongo/client/dbclient.h>

namespace UniSphere {

namespace TestBed {

class DataSetStorage;
class DataSet2;

/**
 * Helper class for insertion of data into a dataset.
 */
class UNISPHERE_EXPORT DataSetRecord {
public:
  /**
   * Class constructor.
   */
  DataSetRecord();

  /**
   * Constructs a self-inserting dataset record. After the record's
   * destructor is called, the record will be inserted into the specified
   * dataset which must exceed the lifetime of this object.
   *
   * @param dataset Dataset instance to insert into after completion
   */
  explicit DataSetRecord(DataSet2 *dataset);

  /**
   * Class destructor.
   */
  ~DataSetRecord();

  /**
   * Returns the BSON object of this record. After calling this function,
   * the record is cleared.
   */
  mongo::BSONObj getBSON()
  {
    return m_bson->obj();
  }

  /**
   * Overloadable operator for adding key-value data to this record.
   *
   * @param key Key name
   * @param value Value
   */
  template <typename BsonSerializableValue>
  DataSetRecord &operator()(const std::string &key, const BsonSerializableValue &value)
  {
    m_bson->append(key, value);
    return *this;
  }

  /**
   * Overloadable operator for adding key-value data to this record.
   *
   * @param key Key name
   * @param value Value
   */
  DataSetRecord &operator()(const std::string &key, size_t value)
  {
    m_bson->appendIntOrLL(key, static_cast<long long>(value));
    return *this;
  }

  /**
   * Overloadable operator for adding key-value data to this record.
   *
   * @param key Key name
   * @param value Value
   */
  DataSetRecord &operator()(const std::string &key, const NodeIdentifier &value)
  {
    m_bson->append(key, value.hex());
    return *this;
  }
private:
  /// BSON object builder
  boost::shared_ptr<mongo::BSONObjBuilder> m_bson;
  /// Optional reference to a dataset this record was created for
  DataSet2 *m_dataset;
};

/**
 * A dataset is a collection records where each record can contain
 * multiple key-value pairs and values are predefined serializable
 * objects.
 */
class UNISPHERE_EXPORT DataSet2 {
public:
  /*
   * Constructs a dataset.
   *
   * @param id Unique test case identifier
   * @param name Data set name
   */
  DataSet2(const std::string &id, const std::string &name);

  /**
   * Returns a unique identifier of this dataset.
   */
  std::string getId() const { return m_id; }

  /**
   * Returns the name of this dataset.
   */
  std::string getName() const { return m_name; }

  /**
   * Starts adding a new dataset record. After the record is destroyed,
   * all the data is saved into the data set storage.
   */
  DataSetRecord add();

  /**
   * Adds a record to the dataset. After this operation, the record object
   * is cleared.
   *
   * @param record Record object to add to dataset
   */
  void add(DataSetRecord &record);

  /**
   * Exports the dataset to CSV.
   *
   * @param fields Fields to export
   * @param filename Output filename
   * @return Reference to self for command chaining
   */
  DataSet2 &csv(std::initializer_list<std::string> fields,
                const std::string &filename);

  /**
   * Clears this dataset. This method should only be called after it is
   * known that all clients have finished processing the dataset.
   */
  void clear();
private:
  /// Unique dataset identifier
  std::string m_id;
  /// Dataset name
  std::string m_name;
  /// Storage namespace
  std::string m_namespace;
  /// Reference to dataset storage
  DataSetStorage &m_dss;
};

}

}

#endif
