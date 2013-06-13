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
#ifndef UNISPHERE_SOCIAL_RPCCHANNEL_H
#define UNISPHERE_SOCIAL_RPCCHANNEL_H

#include "rpc/channel.hpp"
#include "social/routed_message.h"

namespace UniSphere {

class CompactRouter;

/**
 * The social RPC channel uses the compact router to deliver RPC
 * messages. This enables easy operation over the routing infrastructure.
 */
class UNISPHERE_EXPORT SocialRpcChannel : public RpcChannel<RoutedMessage, RoutingOptions> {
public:
  /**
   * Class constructor.
   *
   * @param router Compact router instance
   */
  SocialRpcChannel(CompactRouter &router);

  /**
   * Sends a response back to request originator.
   *
   * @param msg Source message
   * @param response RPC response to send back
   * @param opts Channel-specific options
   */
  void respond(const RoutedMessage &msg,
               const Protocol::RpcResponse &response,
               const RoutingOptions &opts = RoutingOptions());

  /**
   * Sends a request to a remote node.
   *
   * @param destination Destination node identifier
   * @param request RPC request to send
   * @param opts Channel-specific options
   */
  void request(const NodeIdentifier &destination,
               const Protocol::RpcRequest &request,
               const RoutingOptions &opts = RoutingOptions());
private:
  UNISPHERE_DECLARE_PRIVATE(SocialRpcChannel)
};

}

#endif
