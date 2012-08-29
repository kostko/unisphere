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

#include <boost/thread.hpp>

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
   * Outputs logging info.
   *
   * @param level Log level
   * @param text Text to output
   * @param component Optional component name
   */
  void output(Level level, const std::string &text, const std::string &component = "");
private:
  // Logging mutex
  std::mutex m_mutex;
};

// Logging macros
#ifdef UNISPHERE_DEBUG
#define UNISPHERE_CLOG(context, level, text) (context).logger().output(Logger::Level::level, (text))
#else
#define UNISPHERE_CLOG(context, level, text)
#endif

}

#endif
