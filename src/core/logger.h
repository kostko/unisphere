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
#ifndef UNISPHERE_CORE_LOGGER_H
#define UNISPHERE_CORE_LOGGER_H

#include "globals.h"

#include <iostream>

namespace UniSphere {

/**
 * A very simple logging subsystem used only when debugging is enabled
 * to test the inner workins of the framework. When debug mode is disabled
 * no logging takes place.
 */
class UNISPHERE_EXPORT Logger {
public:
  /**
   * Possible log levels.
   */
  enum class Level {
    Info,
    Warning,
    Error
  };
  
  /**
   * Class constructor.
   */
  Logger();

  /**
   * Returns the stream interface to this logger.
   */
  std::ostream &stream();
  
  /**
   * Outputs logging info.
   *
   * @param level Log level
   * @param text Text to output
   * @param component Optional component name
   */
  void output(Level level, const std::string &text, const std::string &component = "");

  /**
   * Stream manipulator to set the logger component name.
   */
  struct Component {
    std::string component;
  };
private:
  UNISPHERE_DECLARE_PRIVATE(Logger)
};

/**
 * Sets the current component when writing to the logger stream.
 */
UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &os, const Logger::Component &component);

/**
 * Sets the current log level when writing to the logger stream.
 */
UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &os, const Logger::Level &level);

/**
 * Forward all stream-like operations on the logger object to the
 * underlying logger stream.
 */
template<typename T>
UNISPHERE_EXPORT std::ostream &operator<<(Logger &logger, const T &t)
{
  std::ostream &s = logger.stream();
  s << t;
  return s;
}

// Logging macros
#ifdef UNISPHERE_DEBUG
#define UNISPHERE_CLOG(context, level, text) (context).logger().output(Logger::Level::level, (text))
#else
#define UNISPHERE_CLOG(context, level, text)
#endif

}

#endif
