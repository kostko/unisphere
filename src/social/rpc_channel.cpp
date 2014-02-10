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
#include "social/rpc_channel.h"
#include "social/compact_router.h"
#include "core/context.h"

namespace UniSphere {

class SocialRpcChannelPrivate {
public:
  /**
   * Class constructor.
   */
  SocialRpcChannelPrivate(SocialRpcChannel &channel,
                          CompactRouter &router);

  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const RoutedMessage &msg);
public:
  UNISPHERE_DECLARE_PUBLIC(SocialRpcChannel)

  /// Compact router instance
  CompactRouter &m_router;
};

SocialRpcChannelPrivate::SocialRpcChannelPrivate(SocialRpcChannel &channel,
                                                 CompactRouter &router)
  : q(channel),
    m_router(router)
{
  router.signalDeliverMessage.connect(boost::bind(&SocialRpcChannelPrivate::messageDelivery, this, _1));
}

void SocialRpcChannelPrivate::messageDelivery(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine))
    return;

  switch (msg.payloadType()) {
    case static_cast<std::uint32_t>(RpcMessageType::Request): {
      Protocol::RpcRequest request = message_cast<Protocol::RpcRequest>(msg);
      q.signalDeliverRequest(request, msg);
      break;
    }
    case static_cast<std::uint32_t>(RpcMessageType::Response): {
      Protocol::RpcResponse response = message_cast<Protocol::RpcResponse>(msg);
      q.signalDeliverResponse(response, msg);
      break;
    }
  }
}

SocialRpcChannel::SocialRpcChannel(CompactRouter &router)
  : d(new SocialRpcChannelPrivate(*this, router)),
    RpcChannel(router.context())
{
}

void SocialRpcChannel::respond(const RoutedMessage &msg,
                               const Protocol::RpcResponse &response,
                               const RoutingOptions &opts)
{
  // Send the RPC message back to the source node
  d->m_router.route(
    static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine),
    msg.sourceNodeId(),
    msg.sourceAddress(),
    static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine),
    static_cast<std::uint32_t>(RpcMessageType::Response),
    response,
    opts
  );
}

void SocialRpcChannel::request(const NodeIdentifier &destination,
                               const Protocol::RpcRequest &request,
                               const RoutingOptions &opts)
{
  // Send the RPC message
  d->m_router.route(
    static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine),
    destination,
    LandmarkAddress(),
    static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine),
    static_cast<std::uint32_t>(RpcMessageType::Request),
    request,
    opts
  );
}

}
