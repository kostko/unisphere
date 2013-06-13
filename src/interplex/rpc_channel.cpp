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
#include "interplex/rpc_channel.h"
#include "interplex/link_manager.h"
#include "core/context.h"

namespace UniSphere {

class InterplexRpcChannelPrivate {
public:
  /**
   * Class constructor.
   */
  InterplexRpcChannelPrivate(InterplexRpcChannel &channel,
                             LinkManager &manager);

  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const Message &msg);
public:
  UNISPHERE_DECLARE_PUBLIC(InterplexRpcChannel)

  /// Link manager instance
  LinkManager &m_manager;
};

InterplexRpcChannelPrivate::InterplexRpcChannelPrivate(InterplexRpcChannel &channel,
                                                       LinkManager &manager)
  : q(channel),
    m_manager(manager)
{
  manager.signalMessageReceived.connect(boost::bind(&InterplexRpcChannelPrivate::messageDelivery, this, _1));
}

void InterplexRpcChannelPrivate::messageDelivery(const Message &msg)
{
  switch (msg.type()) {
    case Message::Type::Interplex_RPC_Request: {
      Protocol::RpcRequest request = message_cast<Protocol::RpcRequest>(msg);
      q.signalDeliverRequest(request, msg);
      break;
    }
    case Message::Type::Interplex_RPC_Response: {
      Protocol::RpcResponse response = message_cast<Protocol::RpcResponse>(msg);
      q.signalDeliverResponse(response, msg);
      break;
    }
  }
}

InterplexRpcChannel::InterplexRpcChannel(LinkManager &manager)
  : d(new InterplexRpcChannelPrivate(*this, manager)),
    RpcChannel(manager.context())
{
}

void InterplexRpcChannel::respond(const Message &msg,
                                  const Protocol::RpcResponse &response,
                                  const MessageOptions &opts)
{
  // Send the RPC message back to the source node
  d->m_manager.send(msg.originator(), Message(Message::Type::Interplex_RPC_Response, response));
}

void InterplexRpcChannel::request(const NodeIdentifier &destination,
                                  const Protocol::RpcRequest &request,
                                  const MessageOptions &opts)
{
  // Send the RPC message
  if (opts.contact.isNull())
    d->m_manager.send(destination, Message(Message::Type::Interplex_RPC_Request, request));
  else
    d->m_manager.send(opts.contact, Message(Message::Type::Interplex_RPC_Request, request));
}

}
