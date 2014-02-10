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
#ifndef UNISPHERE_RPC_EXCEPTIONS_H
#define UNISPHERE_RPC_EXCEPTIONS_H

#include "core/exception.h"

namespace UniSphere {

/**
 * RPC error codes.
 */
enum class RpcErrorCode : std::uint32_t {
  MethodNotFound  = 0x01,
  RequestTimedOut = 0x02,
  BadRequest      = 0x03,
  NoAuthorization = 0x04,
};

/**
 * An RPC exception can be raised by RPC method implementations and
 * cause an error message to be sent back as a reply.
 */
class UNISPHERE_EXPORT RpcException : public Exception {
public:
  /**
   * Constructs an RPC exception.
   *
   * @param code Error code
   * @param msg Error message
   */
  RpcException(RpcErrorCode code, const std::string &msg = "");

  /**
   * Class desctructor.
   */
  ~RpcException() noexcept {};

  /**
   * Returns the error code.
   */
  inline RpcErrorCode code() const { return m_code; }

  /**
   * Returns the error message.
   */
  inline const std::string &message() const { return m_message; }
private:
  /// RPC error code
  RpcErrorCode m_code;
  /// RPC error message
  std::string m_message;
};

}

#endif
