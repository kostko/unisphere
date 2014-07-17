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
#include "testbed/exceptions.h"

namespace UniSphere {

namespace TestBed {

const std::string DataSetStorage::Namespace = "unisphere_testbed";

DataSetStorage::DataSetStorage()
{
}

void DataSetStorage::setConnectionString(const std::string &cs)
{
  std::string error;
  m_cs = mongo::ConnectionString::parse(cs, error);
  if (!m_cs.isValid())
    throw ConnectionStringError(error);
}

mongo::ConnectionString DataSetStorage::getConnectionString() const
{
  return m_cs;
}

void DataSetStorage::initialize()
{
  try {
    mongo::ScopedDbConnection db(getConnectionString());
    db.done();
  } catch (mongo::UserException &e) {
    throw DataSetStorageConnectionFailed(e.toString());
  }
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
