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
#ifndef UNISPHERE_RPC_SERVICE_H
#define UNISPHERE_RPC_SERVICE_H

#include "rpc/call.hpp"

namespace UniSphere {

/**
 * An RPC service is a convenience class that enables calling the same
 * service multiple times without specifying destination and options
 * each time.
 */
template <typename Channel>
class UNISPHERE_EXPORT RpcService {
public:
  /**
   * Creates an invalid service. Calling such a service will do nothing.
   */
  RpcService()
    : m_engine(nullptr)
  {}

  /**
   * Class constructor.
   *
   * @param engine RPC engine instance
   * @param destination Destination identifier
   * @param opts Channel-specific options
   */
  RpcService(RpcEngine<Channel> &engine,
             const NodeIdentifier &destination,
             const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
    : m_engine(&engine),
      m_destination(destination),
      m_options(opts)
  {}

  /**
   * Calls a remote procedure.
   *
   * @param method Method name
   * @param request Request payload
   * @param success Success callback
   * @param failure Failure callback
   */
  template<typename RequestType, typename ResponseType>
  void call(const std::string &method,
            const RequestType &request,
            std::function<void(const ResponseType&, const typename Channel::message_type&)> success,
            RpcResponseFailure failure = nullptr)
  {
    if (m_destination.isNull())
      return;
    m_engine->call<RequestType, ResponseType>(m_destination, method, request, success, failure, m_options);
  }
  
  /**
   * Calls a remote procedure without confirmation.
   *
   * @param destination Destination key
   * @param method Method name
   * @param request Request payload
   * @param opts Call options
   */
  template<typename RequestType>
  void call(const std::string &method,
            const RequestType &request = RequestType(),
            const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
  {
    if (m_destination.isNull())
      return;
    m_engine->call<RequestType>(m_destination, method, request, m_options);
  }
private:
  /// RPC engine
  RpcEngine<Channel> *m_engine;
  /// Service destination
  NodeIdentifier m_destination;
  /// Service options
  RpcCallOptions<Channel> m_options;
};

}

#endif
