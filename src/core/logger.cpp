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

namespace UniSphere {

Logger::Logger()
{
}

void Logger::output(Level level, const std::string &text)
{
  boost::lock_guard<boost::mutex> g(m_mutex);
  
  std::cout << boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time());
  std::cout << " ";
  
  switch (level) {
    case Level::Info: std::cout    << "[INFO   ]"; break;
    case Level::Warning: std::cout << "[WARNING]"; break;
    case Level::Error: std::cout   << "[ERROR  ]"; break;
  }
  
  std::cout << " " << text << std::endl;
}

}
