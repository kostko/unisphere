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
#include "testbed/dataset.h"

#include <map>
#include <list>

#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

namespace UniSphere {

namespace TestBed {

/// A single data record contains multiple key-value pairs
typedef std::map<std::string, DataSet::ValueType> Record;

class DataSetPrivate {
public:
  /// Mutex protecting the data structure
  mutable std::recursive_mutex m_mutex;
  /// Dataset name
  std::string m_name;
  /// A list of data records
  std::list<Record> m_records;
};

DataSet::DataSet()
  : d(new DataSetPrivate)
{
}

DataSet::DataSet(const std::string &name)
  : DataSet()
{
  d->m_name = name;
}

DataSet::DataSet(DataSet &&dataset)
  : d(dataset.d)
{
  dataset.d = boost::make_shared<DataSetPrivate>();
}

DataSet &DataSet::operator=(DataSet &&dataset)
{
  std::swap(d, dataset.d);
  return *this;
}

std::string DataSet::getName() const
{
  return d->m_name;
}

void DataSet::add(std::initializer_list<Element> elements)
{
  RecursiveUniqueLock lock(d->m_mutex);
  Record record;
  for (const Element &element : elements)
    record.insert({{ element.key, element.value }});

  d->m_records.push_back(record);
}

void DataSet::add(const DataSet &dataset)
{
  RecursiveUniqueLock lock(d->m_mutex);
  RecursiveUniqueLock lock2(dataset.d->m_mutex);

  for (const Record &record : dataset.d->m_records)
    d->m_records.push_back(record);
}

void DataSet::moveFrom(DataSet &dataset)
{
  RecursiveUniqueLock lock(d->m_mutex);
  RecursiveUniqueLock lock2(dataset.d->m_mutex);
  
  for (Record &record : dataset.d->m_records)
    d->m_records.push_back(std::move(record));
}

size_t DataSet::size() const
{
  return d->m_records.size();
}

template <typename Archive>
void DataSet::serialize(Archive &ar, const unsigned int)
{
  RecursiveUniqueLock lock(d->m_mutex);
  ar & d->m_records;
}

// Explicit instantiation for text archives
template void DataSet::serialize<boost::archive::text_iarchive>(
  boost::archive::text_iarchive&, const unsigned int);

template void DataSet::serialize<boost::archive::text_oarchive>(
  boost::archive::text_oarchive&, const unsigned int);

}

}
