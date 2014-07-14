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
#include "testbed/dataset/dataset.h"
#include "testbed/dataset/storage.h"
#include "testbed/test_bed.h"

#include <fstream>

namespace UniSphere {

namespace TestBed {

DataSetRecordBuilder::DataSetRecordBuilder()
  : m_bson(boost::make_shared<mongo::BSONObjBuilder>()),
    m_dataset(nullptr)
{
}

DataSetRecordBuilder::DataSetRecordBuilder(DataSet2 *dataset)
  : DataSetRecordBuilder()
{
  m_dataset = dataset;
}

DataSetRecordBuilder::~DataSetRecordBuilder()
{
  if (m_dataset)
    m_dataset->add(*this);
}

DataSet2::DataSet2(const std::string &id, const std::string &name)
  : m_id(name + id),
    m_name(name),
    m_namespace("unisphere_testbed.datasets_" + m_id),
    m_dss(TestBed::getGlobalTestbed().getDataSetStorage())
{
}

DataSetRecordBuilder DataSet2::add()
{
  return DataSetRecordBuilder(this);
}

void DataSet2::add(DataSetRecordBuilder &record)
{
  boost::this_thread::disable_interruption di;
  try {
    mongo::ScopedDbConnection db(m_dss.getConnectionString());
    db->insert(m_namespace, record.getBSON());
    db.done();
  } catch (mongo::UserException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "UserException raised by MongoDB driver on insert: " << e.what();
  } catch (mongo::OperationException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "OperationException raised by MongoDB driver on insert: " << e.what();
  }
}

DataSet2 &DataSet2::csv(std::initializer_list<std::string> fields,
                        const std::string &filename)
{
  boost::this_thread::disable_interruption di;
  std::ofstream file;
  file.open(filename);

  // Output column list
  for (const std::string &field : fields) {
    file << field;
    file << "\t";
  }
  file << "\n";

  // Output data
  try {
    mongo::ScopedDbConnection db(m_dss.getConnectionString());
    auto cursor = db->query(m_namespace, mongo::Query().sort("_id"));
    while (cursor->more()) {
      mongo::BSONObj record = cursor->next();
      for (const std::string &field : fields) {
        if (record.hasField(field)) {
          mongo::BSONElement element = record[field];
          switch (element.type()) {
            case mongo::NumberDouble: file << element.Double(); break;
            case mongo::String: file << element.String(); break;
            case mongo::Bool: file << element.Bool() ? 1 : 0; break;
            case mongo::NumberInt: file << element.Int(); break;
            case mongo::NumberLong: file << element.Long(); break;
            default: file << "-"; break;
          }
        } else {
          file << "-";
        }

        file << "\t";
      }
      file << "\n";
    }
    db.done();
  } catch (mongo::UserException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "UserException raised by MongoDB driver on export: " << e.what();
  } catch (mongo::OperationException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "OperationException raised by MongoDB driver on export: " << e.what();
  }

  return *this;
}

void DataSet2::clear()
{
  boost::this_thread::disable_interruption di;
  try {
    mongo::ScopedDbConnection db(m_dss.getConnectionString());
    db->dropCollection(m_namespace);
    db.done();
  } catch (mongo::UserException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "UserException raised by MongoDB driver on clear: " << e.what();
  } catch (mongo::OperationException &e) {
    Logger logger;
    BOOST_LOG_SEV(logger, log::error) << "OperationException raised by MongoDB driver on clear: " << e.what();
  }
}

}

}
