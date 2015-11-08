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
#include "testbed/dataset/storage.h"
#include "testbed/dataset/processor.h"
#include "testbed/exceptions.h"

namespace UniSphere {

namespace TestBed {

class DataSetStoragePrivate {
public:
  DataSetStoragePrivate(DataSetStorage &dss);
public:
  /// Connection string for the storage server
  mongo::ConnectionString m_cs;
  /// Dataset processor instance
  DataSetProcessor m_processor;
};

DataSetStoragePrivate::DataSetStoragePrivate(DataSetStorage &dss)
  : m_processor(dss)
{
}

const std::string DataSetStorage::Namespace = "unisphere_testbed";

DataSetStorage::DataSetStorage()
  : d(new DataSetStoragePrivate(*this))
{
}

void DataSetStorage::setConnectionString(const std::string &cs)
{
  std::string error;
  d->m_cs = mongo::ConnectionString::parse(cs, error);
  if (!d->m_cs.isValid())
    throw ConnectionStringError(error);
}

mongo::ConnectionString DataSetStorage::getConnectionString() const
{
  return d->m_cs;
}

DataSetProcessor &DataSetStorage::getProcessor()
{
  return d->m_processor;
}

void DataSetStorage::initialize()
{
  try {
    BOOST_ASSERT(mongo::client::initialize().isOk());
    mongo::ScopedDbConnection db(getConnectionString());
    db.done();
  } catch (mongo::UserException &e) {
    throw DataSetStorageConnectionFailed(e.toString());
  }

  // Initialize the dataset processor thread
  d->m_processor.initialize();
}

void DataSetStorage::clear()
{
  try {
    mongo::ScopedDbConnection db(getConnectionString());
    db->dropDatabase(DataSetStorage::Namespace);
    db.done();
  } catch (mongo::UserException &e) {
    throw DataSetStorageConnectionFailed(e.toString());
  }
}

}

}
