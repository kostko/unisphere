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

#include <boost/log/core.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/manipulators/to_log.hpp>

namespace UniSphere {

// Logging
namespace logging = boost::log;

namespace log {

enum LogSeverityLevel {
  normal,
  warning,
  error
};

}

namespace LogTags {
  class Severity;
}

/**
 * Puts a human-readable log severity level to the logging stream.
 */
inline logging::formatting_ostream &operator<<(logging::formatting_ostream &stream,
                                               logging::to_log_manip<log::LogSeverityLevel, LogTags::Severity> const &manip)
{
  static const char *strings[] =
  {
    "normal ",
    "warning",
    "error  "
  };

  log::LogSeverityLevel level = manip.get();
  if (static_cast<std::size_t>(level) < sizeof(strings) / sizeof(*strings))
    stream << strings[level];
  else
    stream << static_cast<int>(level);

  return stream;
}

/// Defined logger type
typedef boost::log::sources::severity_channel_logger<log::LogSeverityLevel, std::string> Logger;

}

#endif
