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
#ifndef UNISPHERE_CORE_LOGGING_H
#define UNISPHERE_CORE_LOGGING_H

#include <chrono>
#include <boost/log/core.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/manipulators/to_log.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>

namespace UniSphere {

// Logging
namespace logging = boost::log;

namespace LogTags {
  class Severity;
}

namespace log {

enum LogSeverityLevel {
  normal,
  warning,
  error,
  debug,
  profiling
};

/**
 * Puts a human-readable log severity level to the logging stream.
 */
inline logging::formatting_ostream &operator<<(logging::formatting_ostream &stream,
                                               logging::to_log_manip<LogSeverityLevel, LogTags::Severity> const &manip)
{
  static const char *strings[] =
  {
    "NORM",
    "WARN",
    "ERRR",
    "DEBG",
    "PROF"
  };

  LogSeverityLevel level = manip.get();
  if (static_cast<std::size_t>(level) < sizeof(strings) / sizeof(*strings))
    stream << strings[level];
  else
    stream << static_cast<int>(level);

  return stream;
}

}

/// Defined logger type
typedef logging::sources::severity_channel_logger<log::LogSeverityLevel, std::string> Logger;

}

#ifdef UNISPHERE_PROFILE

#define UNISPHERE_LOG_PROFILING_START(logger, name) \
  BOOST_LOG_SEV(logger, log::profiling) \
    << boost::log::add_value("ProfileTimePoint", std::chrono::high_resolution_clock::now()) \
    << boost::log::add_value("ProfileName", std::string(#name))

#define UNISPHERE_LOG_PROFILING_TAG(logger, name, tag) \
  BOOST_LOG_SEV(logger, log::profiling) \
    << boost::log::add_value("ProfileTag", std::string(#tag)) \
    << boost::log::add_value("ProfileName", std::string(#name))

#define UNISPHERE_LOG_PROFILING_END(logger, name) \
  BOOST_LOG_SEV(logger, log::profiling) \
    << boost::log::add_value("ProfileTimePoint", std::chrono::high_resolution_clock::now()) \
    << boost::log::add_value("ProfileEnd", true) \
    << boost::log::add_value("ProfileName", std::string(#name))

#else
#define UNISPHERE_LOG_PROFILING_START(logger, name)
#define UNISPHERE_LOG_PROFILING_TAG(logger, name, tag)
#define UNISPHERE_LOG_PROFILING_END(logger, name)
#endif

#endif
