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

#include <boost/serialization/access.hpp>
#include <boost/variant.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * A dataset is a collection records where each record can contain
 * multiple key-value pairs and values are predefined serializable objects.
 */
class UNISPHERE_EXPORT DataSet {
  friend class boost::serialization::access;
public:
  /**
   * Possible values that a dataset may hold.
   */
  typedef boost::variant<
    int,
    long,
    unsigned int,
    unsigned long,
    double,
    std::string
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

  /**
   * Constructs an empty dataset.
   */
  DataSet();

  /**
   * Constructs an empty dataset.
   *
   * @paran name Dataset name
   */
  DataSet(const std::string &name);

  DataSet(DataSet &&dataset);
  DataSet &operator=(DataSet &&dataset);

  DataSet(const DataSet&) = delete;
  DataSet &operator=(const DataSet&) = delete;

  /**
   * Returns the dataset's name.
   */
  std::string getName() const;

  /**
   * Adds a single new record to the data set.
   *
   * @param elements Record elements (key-value pairs)
   */
  void add(std::initializer_list<Element> elements);

  /**
   * Adds another data set to this data set. All records from the source
   * dataset are copied into this one.
   *
   * @param dataset Source dataset
   */
  void add(const DataSet &dataset);

  /**
   * Moves records from source data set to this data set. Source data set
   * is empty after this function returns.
   *
   * @param dataset Source data set
   */
  void moveFrom(DataSet &dataset);

  /**
   * Returns the number of records in the dataset.
   */
  size_t size() const;
private:
  /**
   * Serialization support.
   */
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int);
private:
  UNISPHERE_DECLARE_PRIVATE(DataSet)
};

}

}

#endif
