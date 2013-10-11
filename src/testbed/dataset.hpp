/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "core/serialize_tuple.h"

#include <map>
#include <list>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/variant.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace UniSphere {

namespace TestBed {

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
    std::uint64_t,
    std::string,
    boost::posix_time::ptime,
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

  /// A field that maps a value to a column
  typedef std::tuple<std::uint8_t, ValueType> TypedValueField;
  /// Fields in internal record should always be ordered by column id for faster lookup
  typedef std::vector<TypedValueField> InternalRecord;
  /// A list of records
  typedef std::list<InternalRecord> InternalRecordList;
  /// A map of columns to/from identifiers
  typedef boost::bimap<
    boost::bimaps::unordered_set_of<std::string>,
    boost::bimaps::unordered_set_of<std::uint8_t>
  > InternalColumnMap;

  /**
   * A wrapper class for representing the internal vector-based record
   * as a map-based record.
   */
  class Record {
  public:
    /**
     * Constructs a record wrapper.
     *
     * @param columns Internal column map
     * @param record Internal record to wrap
     */
    Record(const InternalColumnMap &columns, const InternalRecord &record)
      : m_columns(columns),
        m_record(record)
    {
    }

    /**
     * A wrapper for find that emulates map-like traversal by column name.
     *
     * @param column Column name
     */
    typename InternalRecord::const_iterator find(const std::string &column) const
    {
      auto it = m_columns.left.find(column);
      if (it == m_columns.left.end())
        return m_record.end();

      return std::equal_range(
        m_record.begin(),
        m_record.end(),
        std::make_tuple(it->second, ValueType(false)),
        [](const TypedValueField &a, const TypedValueField &b) -> bool {
          return std::get<0>(a) < std::get<0>(b);
        }
      ).first;
    }

    /**
     * Returns an iterator pointing past all the columns.
     */
    typename InternalRecord::const_iterator end() const
    {
      return m_record.end();
    }

    /**
     * A wrapper for at that emulates map-like access by column name. If the
     * column has not value set, this method throws std::out_of_range.
     *
     * @param column Column name
     * @return Value of that column
     */
    const ValueType &at(const std::string &column) const
    {
      auto it = find(column);
      if (it == end())
        throw std::out_of_range(column);
      
      return std::get<1>(*it);
    }
  private:
    /// Reference to internal column map
    const InternalColumnMap &m_columns;
    /// Reference to the wrapped record
    const InternalRecord &m_record;
  };

  /// Iterator type that wraps all internal records into records
  typedef boost::transform_iterator<
    std::function<Record(const InternalRecord&)>,
    typename InternalRecordList::const_iterator
  > const_iterator;

  /**
   * Constructs an empty dataset.
   */
  DataSet()
    : m_nextColumnId(0)
  {}

  /**
   * Constructs an empty dataset.
   *
   * @paran name Dataset name
   */
  DataSet(const std::string &name)
    : m_name(name),
      m_nextColumnId(0)
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
    InternalRecord record;
    for (const Element &element : elements) {
      record.push_back(std::make_tuple(getColumnKey(element.key), element.value));
    }

    std::sort(record.begin(), record.end(),
      [](const TypedValueField &a, const TypedValueField &b) -> bool {
        return std::get<0>(a) < std::get<0>(b);
      });
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

    for (const InternalRecord &record : dataset.m_records)
      m_records.push_back(remapColumns(dataset.m_columns, record));
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

    for (InternalRecord &record : dataset.m_records)
      m_records.push_back(remapColumns(dataset.m_columns, record, true));
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
  const_iterator begin() const
  {
    return const_iterator(m_records.begin(), [this](const InternalRecord &record) -> Record {
      return Record(m_columns, record);
    });
  }

  /**
   * Returns an iterator past the end of the record list.
   */
  const_iterator end() const
  {
    return const_iterator(m_records.end(), [this](const InternalRecord &record) -> Record {
      return Record(m_columns, record);
    });
  }
private:
  /**
   * Determines or assigns a column key.
   *
   * @param column Column name
   * @return Column key
   */
  std::uint8_t getColumnKey(const std::string &column)
  {
    RecursiveUniqueLock lock(m_mutex);
    auto it = m_columns.left.find(column);
    if (it == m_columns.left.end()) {
      std::uint8_t columnKey = m_nextColumnId++;
      m_columns.left.insert({ column, columnKey });
      return columnKey;
    } else {
      return it->second;
    }
  }

  /**
   * Remaps column identifiers from another dataset so they become
   * compatible with this dataset.
   *
   * @param columns Internal column map of the other dataset
   * @param record Record to remap
   * @param move Should the values be moved instead of copied
   * @return Internal record with remapped columns
   */
  InternalRecord remapColumns(InternalColumnMap &columns,
                              InternalRecord &record,
                              bool move = false)
  {
    InternalRecord result;
    for (auto &p : record) {
      std::uint8_t mappedKey = getColumnKey(columns.right.at(std::get<0>(p)));
      if (mappedKey != std::get<0>(p))
        result.push_back(std::make_tuple(mappedKey, move ? std::move(std::get<1>(p)) : std::get<1>(p)));
      else
        result.push_back(move ? std::move(p) : p);
    }
    return result;
  }

  /**
   * Serialization support.
   */
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int)
  {
    RecursiveUniqueLock lock(m_mutex);
    ar & m_nextColumnId;
    ar & m_columns;
    ar & m_records;
  }
private:
  /// Mutex protecting the data structure
  mutable std::recursive_mutex m_mutex;
  /// Dataset name
  std::string m_name;
  /// Next column mapping
  std::uint8_t m_nextColumnId;
  /// Column name to identifier mappings
  InternalColumnMap m_columns;
  /// A list of data records
  InternalRecordList m_records;
};

}

}

#endif
