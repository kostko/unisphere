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

#include <boost/iterator/iterator_facade.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

namespace UniSphere {

namespace TestBed {

class DataSetStorage;
class DataSet2;

/**
 * An immutable dataset record.
 */
class UNISPHERE_EXPORT DataSetRecord {
public:
  friend class DataSetRecordIterator;

  /**
   * Class constructor.
   */
  DataSetRecord();

  /**
   * Constructs a dataset record from BSON data.
   *
   * @param bson BSON data
   */
  DataSetRecord(const mongo::BSONObj &bson);

  /**
   * Returns true if the record contains a specific field.
   *
   * @param name Field name
   * @return True if record contains the field, false otherwise
   */
  bool hasField(const std::string &name) const { return m_bson.hasField(name); }

  /**
   * Retrieves a given field belonging to this record.
   *
   * @param name Field name
   * @param def Default field value in case it doesn't exist
   */
  template <typename T>
  T field(const std::string &name, const T &def = T()) const
  {
    if (!hasField(name))
      return def;

    T value;
    extractField(m_bson.getField(name), value);
    return value;
  }
protected:
  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, std::string &value) const
  {
    value = element.String();
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, int &value) const
  {
    value = element.Int();
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, long int &value) const
  {
    value = element.Long();
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, long long &value) const
  {
    value = element.Long();
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, bool &value) const
  {
    value = element.Bool();
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, NodeIdentifier &value) const
  {
    value = NodeIdentifier(element.String(), NodeIdentifier::Format::Hex);
  }

  /**
   * Field extractor.
   */
  void extractField(const mongo::BSONElement &element, boost::posix_time::ptime &value) const
  {
    value = boost::posix_time::from_time_t(0) + boost::posix_time::milliseconds(element.Date());
  }

  /**
   * Field extractor.
   */
  template <typename T>
  void extractField(const mongo::BSONElement &element, std::vector<T> &value) const
  {
    for (auto it = element.Obj().begin(); it.more();) {
      mongo::BSONElement e = it.next();
      T subvalue;
      extractField(e, subvalue);
      value.push_back(subvalue);
    }
  }

  /**
   * Field extractor.
   */
  template <typename T>
  void extractField(const mongo::BSONElement &element, std::list<T> &value) const
  {
    for (auto it = element.Obj().begin(); it.more();) {
      mongo::BSONElement e = it.next();
      T subvalue;
      extractField(e, subvalue);
      value.push_back(subvalue);
    }
  }

  /**
   * Field extractor.
   */
  template <typename... T>
  void extractField(const mongo::BSONElement &element, boost::adjacency_list<T...> &value) const
  {
    int bufferLength = 0;
    const char *data = element.binData(bufferLength);
    std::string buffer(data, bufferLength);
    std::stringstream is(buffer);
    boost::iostreams::filtering_istream f;
    f.push(boost::iostreams::gzip_decompressor());
    f.push(is);
    boost::archive::text_iarchive archive(f);
    archive >> value;
  }
private:
  /// Underlying BSON object
  mongo::BSONObj m_bson;
};

/**
 * Iterator over dataset records.
 */
class UNISPHERE_EXPORT DataSetRecordIterator : public boost::iterator_facade<
  DataSetRecordIterator,
  DataSetRecord const,
  boost::single_pass_traversal_tag
> {
public:
  /**
   * Class constructor.
   */
  DataSetRecordIterator();

  /**
   * Default move constructor.
   */
  DataSetRecordIterator(DataSetRecordIterator &&other) = default;

  /**
   * Class constructor
   *
   * @param db Database instance
   * @param cursor Cursor instance
   */
  DataSetRecordIterator(std::unique_ptr<mongo::ScopedDbConnection> &&db,
                        std::unique_ptr<mongo::DBClientCursor> &&cursor);

  /**
   * Class destructor.
   */
  ~DataSetRecordIterator();
private:
  friend class boost::iterator_core_access;

  /**
   * Iterator increment operation implementation.
   */
  void increment();

  /**
   * Iterator equality check implementation.
   */
  bool equal(const DataSetRecordIterator &other) const;

  /**
   * Iterator dereference operation implementation.
   */
  const DataSetRecord &dereference() const;
private:
  /// Underlying MongoDB connection
  std::unique_ptr<mongo::ScopedDbConnection> m_db;
  /// Underlying MongoDB cursor
  std::unique_ptr<mongo::DBClientCursor> m_cursor;
  /// Last fetched record
  DataSetRecord m_record;
};

/**
 * Helper class for insertion of data into a dataset.
 */
class UNISPHERE_EXPORT DataSetRecordBuilder {
public:
  /**
   * Class constructor.
   */
  DataSetRecordBuilder();

  /**
   * Constructs a self-inserting dataset record. After the record's
   * destructor is called, the record will be inserted into the specified
   * dataset which must exceed the lifetime of this object.
   *
   * @param dataset Dataset instance to insert into after completion
   */
  explicit DataSetRecordBuilder(DataSet2 *dataset);

  /**
   * Class destructor.
   */
  ~DataSetRecordBuilder();

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
  DataSetRecordBuilder &operator()(const std::string &key, const BsonSerializableValue &value)
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
  DataSetRecordBuilder &operator()(const std::string &key, long int value)
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
  DataSetRecordBuilder &operator()(const std::string &key, size_t value)
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
  DataSetRecordBuilder &operator()(const std::string &key, const NodeIdentifier &value)
  {
    m_bson->append(key, value.hex());
    return *this;
  }

  /**
   * Overloadable operator for adding key-value data to this record.
   *
   * @param key Key name
   * @param value Value
   */
  DataSetRecordBuilder &operator()(const std::string &key, const boost::posix_time::ptime &value)
  {
    m_bson->appendDate(
      key,
      mongo::Date_t((value - boost::posix_time::from_time_t(0)).total_milliseconds())
    );
    return *this;
  }

  /**
   * Overloadable operator for adding key-value data to this record.
   *
   * @param key Key name
   * @param value Value
   */
  template <typename... T>
  DataSetRecordBuilder &operator()(const std::string &key,
                                   const boost::adjacency_list<T...> &value)
  {
    std::stringstream buffer;
    {
      boost::iostreams::filtering_ostream f;
      f.push(boost::iostreams::gzip_compressor());
      f.push(buffer);
      boost::archive::text_oarchive archive(f);
      archive << value;
    }
    std::string data = buffer.str();
    m_bson->appendBinData(key, data.size(), mongo::bdtCustom, static_cast<const void*>(data.data()));
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
  DataSetRecordBuilder add();

  /**
   * Adds a record to the dataset. After this operation, the record object
   * is cleared.
   *
   * @param record Record object to add to dataset
   */
  void add(DataSetRecordBuilder &record);

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
   * Returns an iterator to the beginning of the dataset.
   */
  DataSetRecordIterator begin() const;

  /**
   * Returns an iterator past the end of the dataset.
   */
  DataSetRecordIterator end() const;

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
