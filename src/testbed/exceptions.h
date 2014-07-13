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
#ifndef UNISPHERE_TESTBED_EXCEPTIONS_H
#define UNISPHERE_TESTBED_EXCEPTIONS_H

#include "core/exception.h"
#include "identity/node_identifier.h"

namespace UniSphere {

namespace TestBed {

/**
 * Generic exception for testbed-related errors.
 */
class UNISPHERE_EXPORT TestBedException : public Exception {
public:
  /**
   * Class constructor.
   *
   * @param message Exception message
   */
  TestBedException(const std::string &message = "")
    : Exception("Testbed Exception: " + message),
      m_message(message)
  {}

  /**
   * Returns the error message.
   */
  inline const std::string &message() const { return m_message; }
private:
  /// Exception message
  std::string m_message;
};

class UNISPHERE_EXPORT ArgumentError : public TestBedException {
public:
  ArgumentError(const std::string &message)
    : TestBedException(message)
  {}
};

class UNISPHERE_EXPORT ScenarioNotFound : public TestBedException {
public:
  ScenarioNotFound(const std::string &name)
    : TestBedException("Scenario '" + name + "' not found!")
  {}
};

class UNISPHERE_EXPORT TopologyLoadingFailed : public TestBedException {
public:
  TopologyLoadingFailed(const std::string &filename)
    : TestBedException("Loading of GraphML topology from '" + filename + "' failed!")
  {}
};

class UNISPHERE_EXPORT TopologyMalformed : public TestBedException {
public:
  TopologyMalformed(const std::string &message)
    : TestBedException("Input GraphML topology is malformed: " + message)
  {}
};

class UNISPHERE_EXPORT VirtualNodeNotFound : public TestBedException {
public:
  VirtualNodeNotFound(const NodeIdentifier &nodeId)
    : TestBedException("Virtual node '" + nodeId.hex() + "' not found!")
  {}
};

class UNISPHERE_EXPORT DataSetNotFound : public TestBedException {
public:
  DataSetNotFound(const std::string &dsName)
    : TestBedException("Dataset '" + dsName + "' not found!")
  {}
};

class UNISPHERE_EXPORT IllegalApiCall : public TestBedException {
public:
  IllegalApiCall()
    : TestBedException("Illegal API call!")
  {}
};

class UNISPHERE_EXPORT ScenarioNotRunning : public TestBedException {
public:
  ScenarioNotRunning()
    : TestBedException("Scenario not running!")
  {}
};

class UNISPHERE_EXPORT ConnectionStringError : public TestBedException {
public:
  ConnectionStringError(const std::string &error)
    : TestBedException("Connection string error: " + error)
  {}
};

}

}

#endif
