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
#include "plexus/router.h"
#include "plexus/bootstrap.h"
#include "interplex/link_manager.h"
#include "src/plexus/core_methods.pb.h"

namespace UniSphere {

Router::Router(LinkManager &manager, Bootstrap &bootstrap)
  : m_manager(manager),
    m_bootstrap(bootstrap),
    m_routes(manager.getLocalNodeId(), Router::bucket_size, Router::sibling_neighbourhood),
    m_rpc(*this)
{
  m_manager.setLinkInitializer(boost::bind(&Router::initializeLink, this, _1));
  m_routes.signalRejoin.connect(boost::bind(&Router::join, this));
  
  // Register core routing RPC methods
  registerCoreRpcMethods();
}

void Router::registerCoreRpcMethods()
{
  // Find node that is destined for the local node
  m_rpc.registerMethod<Protocol::FindNodeRequest, Protocol::FindNodeResponse>("Core.FindNode",
    [this](const Protocol::FindNodeRequest &request, const RoutedMessage &msg) -> Protocol::FindNodeResponse {
      // Return the specified number of sibling nodes
      Protocol::FindNodeResponse response;
      DistanceOrderedTable siblings = m_routes.lookup(msg.destinationKeyId(), request.num_contacts());
      for (const PeerEntry &entry : siblings.table().get<DistanceTag>()) {
        Protocol::Contact *contact = response.add_contacts();
        *contact = entry.contact.toMessage();
      }
      
      return response;
    }
  );
  
  // Find node that is in transit over the local node
  m_rpc.registerInterceptMethod<Protocol::FindNodeRequest>("Core.FindNode",
    [this](const Protocol::FindNodeRequest &request, const RoutedMessage &msg) {
      // TODO Rate limiting
      
    }
  );
  
  // Peer entry exchange for filling up local k-buckets
  m_rpc.registerMethod<Protocol::ExchangeEntriesRequest, Protocol::ExchangeEntriesResponse>("Core.ExchangeEntries",
    [this](const Protocol::ExchangeEntriesRequest, const RoutedMessage &msg) -> Protocol::ExchangeEntriesResponse {
    }
  );
}

void Router::join()
{
  Contact bootstrapContact = m_bootstrap.getBootstrapContact();
  m_bootstrap.signalContactReady.disconnect(boost::bind(&Router::join, this));
  if (bootstrapContact.isNull()) {
    // Bootstrap contact is not yet read, we should be called again when one becomes available
    m_bootstrap.signalContactReady.connect(boost::bind(&Router::join, this));
    return;
  }
  
  m_manager.connect(bootstrapContact);
}

void Router::initializeLink(Link &link)
{
  link.signalEstablished.connect(boost::bind(&Router::linkEstablished, this, _1));
  link.signalDisconnected.connect(boost::bind(&Router::linkLost, this, _1));
  link.signalMessageReceived.connect(boost::bind(&Router::linkMessageReceived, this, _1));
}

void Router::linkEstablished(LinkPtr link)
{
  // Adds the established link to the routing table
  m_routes.add(link);
  
  // TODO Queue our node identifier lookup to insert ourselves into the routing tables
  //      of other nodes
}

void Router::linkLost(LinkPtr link)
{
  // Removes the lost link from the routing table
  m_routes.remove(link->nodeId());
}

void Router::linkMessageReceived(const Message &msg)
{
  if (msg.type() != Message::Type::Plexus_Routed)
    return;
  
  // Deserialize the message header and route the message
  RoutedMessage rmsg(msg);
  rmsg.decrementHopCount();
  route(rmsg);
}

void Router::route(const RoutedMessage &msg)
{
  if (!msg.isValid()) {
    UNISPHERE_LOG(m_manager.context(), Warning, "Router: Dropping invalid message.");
    return;
  }
  
  DistanceOrderedTable nextHops = m_routes.lookup(msg.destinationKeyId(), Router::bucket_size);
  if (nextHops.table().size() == 0) {
    UNISPHERE_LOG(m_manager.context(), Warning, "Router: No route to destination.");
    return;
  }
  
  // TODO The forwarding process should be used to also meet other nodes
  
  // Check if the message is destined to the local node, in this case it should be
  // delivered to an upper layer application/component
  PeerEntry nextHop = *nextHops.table().get<DistanceTag>().begin();
  if (nextHop.nodeId == m_manager.getLocalNodeId()) {
    signalDeliverMessage(msg);
    return;
  } else {
    signalForwardMessage(msg);
    nextHop.link->send(Message(Message::Type::Plexus_Routed, *msg.serialize()));
  }
}

void Router::route(std::uint32_t sourceCompId, const NodeIdentifier &key,
                   std::uint32_t destinationCompId, std::uint32_t type,
                   const google::protobuf::Message &msg)
{
  // First encapsulate the specified application message into a routed message
  RoutedMessage rmsg(m_manager.getLocalNodeId(), sourceCompId, key, destinationCompId, type, msg);
  // Attempt to route the generated message
  route(rmsg);
}


}
