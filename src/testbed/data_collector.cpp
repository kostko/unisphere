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
#include "testbed/data_collector.h"
#include "testbed/test_bed.h"

#include <fstream>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>
#include <boost/thread/tss.hpp>

namespace UniSphere {

namespace TestBed {

class SynchronizedStream;

class DataCollectorPrivate {
public:
  ~DataCollectorPrivate();
public:
  /// Mutex protecting the collector
  mutable std::recursive_mutex m_mutex;
  /// Underlying output stream
  std::ofstream m_stream;
  /// Synchronized stream (one per thread)
  boost::thread_specific_ptr<SynchronizedStream> m_syncedStream;
  /// Number of columns
  int m_columns;
};

class SynchronizedStream : public std::ostream {
private:
  class Buffer : public std::stringbuf {
  public:
    Buffer(DataCollectorPrivate &data)
      : m_data(data)
    {
    }

    ~Buffer()
    {
      pubsync();
    }

    int sync()
    {
      RecursiveUniqueLock lock(m_data.m_mutex);
      if (m_data.m_columns > 0)
        m_data.m_stream << str();
      str("");
      return 0;
    }
  private:
    /// Reference to data collector
    DataCollectorPrivate &m_data;
  };
public:
  SynchronizedStream(DataCollectorPrivate &data)
    : std::ostream(new Buffer(data)),
      m_currentColumn(0)
  {
  }

  ~SynchronizedStream()
  {
    delete rdbuf();
  }
public:
  /// Current column
  int m_currentColumn;
};

DataCollectorPrivate::~DataCollectorPrivate()
{
  RecursiveUniqueLock lock(m_mutex);
  m_stream.close();
  m_columns = 0;
}

DataCollector::DataCollector(const std::string &directory,
                             const std::string &component,
                             std::initializer_list<std::string> columns)
  : d(new DataCollectorPrivate)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_stream.open(
    (boost::format("%1%/%2%-%3%.csv")
      % directory
      % boost::algorithm::replace_all_copy(component, "/", "-")
      % TestBed::getGlobalTestbed().time()
    ).str()
  );
  d->m_columns = columns.size();
  d->m_stream << boost::algorithm::join(columns, ",") << std::endl;
}

std::ostream &DataCollector::stream()
{
  SynchronizedStream *stream = d->m_syncedStream.get();
  if (!stream) {
    stream = new SynchronizedStream(*d);
    d->m_syncedStream.reset(stream);
  }

  return *stream;
}

void DataCollector::nextColumn()
{
  SynchronizedStream &stream = static_cast<SynchronizedStream&>(this->stream());
  if (d->m_columns <= 0)
    return;

  if (++stream.m_currentColumn >= d->m_columns) {
    stream << std::endl;
    stream.m_currentColumn = 0;
  } else {
    stream << ",";
  }
}

DataCollector &operator<<(DataCollector &dc, const std::string &str)
{
  dc.stream() << '"' << boost::algorithm::replace_all_copy(str, "\"", "\\\"") << '"';
  dc.nextColumn();
  return dc;
}

}

}
