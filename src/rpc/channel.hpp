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
#ifndef UNISPHERE_RPC_CHANNEL_H
#define UNISPHERE_RPC_CHANNEL_H

#include "core/globals.h"
#include "identity/node_identifier.h"
#include "src/rpc/rpc.pb.h"

#include <boost/signals2/signal.hpp>

namespace UniSphere {

class Context;

/**
 * RPC message types.
 */
enum class RpcMessageType : std::uint32_t {
  Request   = 0x01,
  Response  = 0x02,
};

/**
 * The RPC channel is an abstract class that should be derived by concrete
 * channel implementations. It is the input/output system for the RPC engine
 * that handles message dispatch via the appropriate mechanism. This way
 * multiple different transport mechanisms can be used with the same RPC
 * engine.
 */
template <typename MessageType, typename OptionsType>
class UNISPHERE_EXPORT RpcChannel {
public:
  /// Defines the lower-level message type of the channel used to encapsulate RPC messages
  typedef MessageType message_type;
  /// Defines the channel-specific options type that can be used to give special delivery options
  typedef OptionsType options_type;
public:
  /**
   * Class constructor.
   *
   * @param context Context instance
   */
  RpcChannel(Context &context)
    : m_context(context)
  {
  }

  /**
   * Returns the context associated with this channel.
   */
  Context &context() const { return m_context; }

  /**
   * Sends a response back to request originator.
   *
   * @param msg Source message
   * @param response RPC response to send back
   * @param opts Channel-specific options
   */
  virtual void respond(const MessageType &msg,
                       const Protocol::RpcResponse &response,
                       const OptionsType &opts = OptionsType()) = 0;

  /**
   * Sends a request to a remote node.
   *
   * @param destination Destination node identifier
   * @param request RPC request to send
   * @param opts Channel-specific options
   */
  virtual void request(const NodeIdentifier &destination,
                       const Protocol::RpcRequest &request,
                       const OptionsType &opts = OptionsType()) = 0;
public:
  /// Signal that gets called when a new request has to be processed by the RPC engine
  boost::signals2::signal<void(const Protocol::RpcRequest&, const MessageType&)> signalDeliverRequest;
  /// Signal that gets called when a new response has to be processed by the RPC engine
  boost::signals2::signal<void(const Protocol::RpcResponse&, const MessageType&)> signalDeliverResponse;
private:
  /// UNISPHERE context instance
  Context &m_context;
};

}

#endif
