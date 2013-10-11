/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_CORE_EXCEPTION_H
#define UNISPHERE_CORE_EXCEPTION_H

#include <exception>

#include "core/globals.h"

namespace UniSphere {

/**
 * All exceptions in UNISPHERE framework derive from this class.
 */
class UNISPHERE_EXPORT Exception : public std::exception {
public:
  /**
   * Class constructor.
   */
  Exception(const std::string &msg = "Unknown error")
  {
    setMessage(msg);
  }
  
  /**
   * Class destructor.
   */
  virtual ~Exception() noexcept
  {}
  
  /**
   * Returns the exception message.
   */
  const char *what() const noexcept
  {
    return m_message.c_str();
  }
protected:
  /**
   * Sets the exception message.
   */
  void setMessage(const std::string &msg)
  {
    m_message = "UNISPHERE ERROR: " + msg;
  }
private:
  // Exception message
  std::string m_message;
};

}

#endif
