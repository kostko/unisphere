/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_INTERPLEX_RPCCHANNEL_H
#define UNISPHERE_INTERPLEX_RPCCHANNEL_H

#include "rpc/channel.hpp"
#include "interplex/message.h"
#include "interplex/contact.h"

namespace UniSphere {

class LinkManager;

class MessageOptions {
public:
  /**
   * Class constructor.
   */
  MessageOptions()
  {}

  /**
   * Forces the message to be delivered to a specific contact.
   */
  MessageOptions &setContact(const Contact &contact) { this->contact = contact; return *this; }
public:
  /// Contact to deliver the message to
  Contact contact;
};

/**
 * The interplex RPC channel can be used to perform RPC requests over
 * the direct link between two peers.
 */
class UNISPHERE_EXPORT InterplexRpcChannel : public RpcChannel<Message, MessageOptions> {
public:
  /**
   * Class constructor.
   *
   * @param manager Link manager instance
   */
  InterplexRpcChannel(LinkManager &manager);

  /**
   * Sends a response back to request originator.
   *
   * @param msg Source message
   * @param response RPC response to send back
   * @param opts Channel-specific options
   */
  void respond(const Message &msg,
               const Protocol::RpcResponse &response,
               const MessageOptions &opts = MessageOptions());

  /**
   * Sends a request to a remote node.
   *
   * @param destination Destination node identifier
   * @param request RPC request to send
   * @param opts Channel-specific options
   */
  void request(const NodeIdentifier &destination,
               const Protocol::RpcRequest &request,
               const MessageOptions &opts = MessageOptions());
private:
  UNISPHERE_DECLARE_PRIVATE(InterplexRpcChannel)
};

}

#endif
