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
#ifndef UNISPHERE_TESTBED_DATASET_H
#define UNISPHERE_TESTBED_DATASET_H

#include "core/globals.h"

#include <map>
#include <list>
#include <boost/serialization/access.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/variant.hpp>

namespace UniSphere {

namespace TestBed {

/// Buffer that contains received datasets pending deserialization
typedef std::list<std::string> DataSetBuffer;

/**
 * A dataset is a collection records where each record can contain
 * multiple key-value pairs and values are predefined serializable objects.
 */
template <typename... AdditionalTypes>
class UNISPHERE_EXPORT DataSet {
  friend class boost::serialization::access;
public:
  /**
   * Possible values that a dataset may hold.
   */
  typedef boost::variant<
    bool,
    int,
    long,
    unsigned int,
    unsigned long,
    double,
    std::string,
    AdditionalTypes...
  > ValueType;

  /**
   * Convenience structure for simpler initialization of a record.
   */
  struct Element {
    /// Elemenet key
    std::string key;
    /// Element value
    ValueType value;
  };

  /// A single data record contains multiple key-value pairs
  typedef std::map<std::string, ValueType> Record;
  /// A list of records
  typedef std::list<Record> RecordList;

  /**
   * Constructs an empty dataset.
   */
  DataSet()
  {}

  /**
   * Constructs an empty dataset.
   *
   * @paran name Dataset name
   */
  DataSet(const std::string &name)
    : m_name(name)
  {
  }

  /**
   * Returns the dataset's name.
   */
  std::string getName() const
  {
    return m_name;
  }

  /**
   * Adds a single new record with a single element to the data set.
   *
   * @param element Record element (key-value pair)
   */
  void add(const Element &element)
  {
    add({ element });
  }

  /**
   * Adds a single new record to the data set.
   *
   * @param elements Record elements (key-value pairs)
   */
  void add(std::initializer_list<Element> elements)
  {
    RecursiveUniqueLock lock(m_mutex);
    Record record;
    for (const Element &element : elements)
      record.insert({{ element.key, element.value }});

    m_records.push_back(record);
  }

  /**
   * Adds another data set to this data set. All records from the source
   * dataset are copied into this one.
   *
   * @param dataset Source dataset
   */
  void add(const DataSet<AdditionalTypes...> &dataset)
  {
    RecursiveUniqueLock lock(m_mutex);
    RecursiveUniqueLock lock2(dataset.m_mutex);

    for (const Record &record : dataset.m_records)
      m_records.push_back(record);
  }

  /**
   * Moves records from source data set to this data set. Source data set
   * is empty after this function returns.
   *
   * @param dataset Source data set
   */
  void moveFrom(DataSet<AdditionalTypes...> &dataset)
  {
    RecursiveUniqueLock lock(m_mutex);
    RecursiveUniqueLock lock2(dataset.m_mutex);

    for (Record &record : dataset.m_records)
      m_records.push_back(std::move(record));
  }

  /**
   * Removes all records from this dataset.
   */
  void clear()
  {
    RecursiveUniqueLock lock(m_mutex);
    m_records.clear();
  }

  /**
   * Returns the number of records in the dataset.
   */
  size_t size() const
  {
    return m_records.size();
  }

  /**
   * Returns an iterator to the beginning of the record list.
   */
  typename RecordList::const_iterator begin() const
  {
    return m_records.begin();
  }

  /**
   * Returns an iterator past the end of the record list.
   */
  typename RecordList::const_iterator end() const
  {
    return m_records.end();
  }
private:
  /**
   * Serialization support.
   */
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int)
  {
    RecursiveUniqueLock lock(m_mutex);
    ar & m_records;
  }
private:
  /// Mutex protecting the data structure
  mutable std::recursive_mutex m_mutex;
  /// Dataset name
  std::string m_name;
  /// A list of data records
  RecordList m_records;
};

}

}

#endif
