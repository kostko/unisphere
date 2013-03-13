/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#include "core/logger.h"

#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/tss.hpp>

namespace UniSphere {

class SynchronizedStream;

class LoggerPrivate {
public:
  void output(Logger::Level level, const std::string &text, const std::string &component, bool newline);
public:
  /// Logging mutex
  mutable std::mutex m_mutex;
  /// Per-thread synchronized buffers
  boost::thread_specific_ptr<SynchronizedStream> m_streams;
};

class SynchronizedStream : public std::ostream {
private:
  class Buffer : public std::stringbuf {
  public:
    Buffer(LoggerPrivate &logger)
      : m_logger(logger),
        m_level(Logger::Level::Info)
    {
    }

    ~Buffer()
    {
      pubsync();
    }

    int sync()
    {
      m_logger.output(m_level, str(), m_component, false);
      str("");
      return 0;
    }
  public:
    /// Current component
    std::string m_component;
    /// Current log level
    Logger::Level m_level;
  private:
    LoggerPrivate &m_logger;
  };
public:
  SynchronizedStream(LoggerPrivate &logger)
    : std::ostream(new Buffer(logger))
  {
  }

  ~SynchronizedStream()
  {
    delete rdbuf();
  }

  void setComponent(const std::string &component)
  {
    static_cast<Buffer*>(rdbuf())->m_component = component;
  }

  void setLevel(Logger::Level level)
  {
    static_cast<Buffer*>(rdbuf())->m_level = level;
  }
};

std::ostream &operator<<(std::ostream &os, const Logger::Component &component)
{
  SynchronizedStream &s = static_cast<SynchronizedStream&>(os);
  s.setComponent(component.component);
}

std::ostream &operator<<(std::ostream &os, const Logger::Level &level)
{
  SynchronizedStream &s = static_cast<SynchronizedStream&>(os);
  s.setLevel(level);
}

void LoggerPrivate::output(Logger::Level level, const std::string &text, const std::string &component, bool newline)
{
  UniqueLock lock(m_mutex);
  
  std::cout << boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());
  std::cout << " ";
  
  switch (level) {
    case Logger::Level::Info: std::cout    << "[INFO   ]"; break;
    case Logger::Level::Warning: std::cout << "[WARNING]"; break;
    case Logger::Level::Error: std::cout   << "[ERROR  ]"; break;
  }
  
  if (!component.empty())
    std::cout << " [" << component << "]";
  else
    std::cout << " [global]";
  
  std::cout << " " << text;
  if (newline)
    std::cout << std::endl;
}

Logger::Logger()
  : d(*new LoggerPrivate)
{
}

std::ostream &Logger::stream()
{
  SynchronizedStream *stream = d.m_streams.get();
  if (!stream) {
    stream = new SynchronizedStream(d);
    d.m_streams.reset(stream);
  }
  
  return *stream;
}

void Logger::output(Level level, const std::string &text, const std::string &component)
{
  d.output(level, text, component, true);
}

}
