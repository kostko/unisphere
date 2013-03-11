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
    m_rpc(*this),
    m_state(State::Init)
{
  m_manager.signalMessageReceived.connect(boost::bind(&Router::messageReceived, this, _1));
  
  // Register core routing RPC methods
  registerCoreRpcMethods();
}

void Router::registerCoreRpcMethods()
{
  // Find node RPC that is destined for the local node
  m_rpc.registerMethod<Protocol::FindNodeRequest, Protocol::FindNodeResponse>("Core.FindNode",
    [this](const Protocol::FindNodeRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::FindNodeResponse> {
      UNISPHERE_LOG(m_manager, Info, "Router: Received Core.FindNode call from " + msg.sourceNodeId().as(NodeIdentifier::Format::Hex));
      
      // TODO Maximum number of sibling nodes
      // Return the specified number of sibling nodes
      Protocol::FindNodeResponse response;
      DistanceOrderedTable siblings = m_routes.lookup(msg.destinationKeyId(), request.num_contacts());
      for (const PeerEntry &entry : siblings.table().get<DistanceTag>()) {
        Protocol::Contact *contact = response.add_contacts();
        if (entry.nodeId == m_manager.getLocalNodeId())
          *contact = m_manager.getLocalContact().toMessage();
        else
          *contact = entry.contact.toMessage();
      }
      
      Contact backContact = Contact::fromMessage(request.local_contact());
      return RpcResponse<Protocol::FindNodeResponse>(
        response,
        RoutingOptions().setDeliverVia(backContact)
      );
    }
  );
  
  // Leave node RPC that signals when a node is leaving
  m_rpc.registerMethod<Protocol::LeaveNodeRequest>("Core.LeaveNode",
    [this](const Protocol::LeaveNodeRequest&, const RoutedMessage &msg, RpcId) {
      UNISPHERE_LOG(m_manager, Info, "Node " + msg.sourceNodeId().as(NodeIdentifier::Format::Hex) + " is leaving.");
      
      // Remove node from routing table
      m_routes.remove(msg.sourceNodeId());
    }
  );
  
  // Find node RPC that is in transit over the local node
  m_rpc.registerInterceptMethod<Protocol::FindNodeRequest>("Core.FindNode",
    [this](const Protocol::FindNodeRequest &request, const RoutedMessage &msg, RpcId rpcId) {
      if (msg.sourceNodeId() == m_manager.getLocalNodeId())
        return;
      
      // TODO Maximum number of key-sibling nodes
      // TODO Rate limiting
      
      // Push the specified number of key-sibling nodes to the source node
      Protocol::ExchangeEntriesRequest backRequest;
      backRequest.set_rpcid(rpcId);
      backRequest.set_destination(msg.destinationKeyId().as(NodeIdentifier::Format::Raw));
      DistanceOrderedTable siblings = m_routes.lookup(msg.destinationKeyId(), request.num_contacts());
      for (const PeerEntry &entry : siblings.table().get<DistanceTag>()) {
        Protocol::Contact *contact = backRequest.add_contacts();
        if (entry.nodeId == m_manager.getLocalNodeId())
          *contact = m_manager.getLocalContact().toMessage();
        else
          *contact = entry.contact.toMessage();
      }
      
      // Perform a back-request without confirmation to the source node
      Contact backContact = Contact::fromMessage(request.local_contact());
      if (backContact.nodeId() != msg.sourceNodeId())
        return;
      
      m_rpc.call<Protocol::ExchangeEntriesRequest>(msg.sourceNodeId(), "Core.ExchangeEntries", backRequest,
        RpcCallOptions().setDeliverVia(backContact)
      );
    }
  );
  
  // Peer entry exchange for filling up local k-buckets
  m_rpc.registerMethod<Protocol::ExchangeEntriesRequest>("Core.ExchangeEntries",
    [this](const Protocol::ExchangeEntriesRequest &request, const RoutedMessage&, RpcId) {
      // Ensure that a recent outgoing RPC with the specified identifier exists
      if (!m_rpc.isRecentCall(request.rpcid())) {
        UNISPHERE_LOG(m_manager, Warning, "RPC method Core.ExchangeEntries called with invalid rpcId!");
        return;
      }
      
      // Queue all contacts to be contacted later
      RecursiveUniqueLock lock(m_mutex);
      for (const Protocol::Contact &ct : request.contacts()) {
        m_pendingContacts.insert(Contact::fromMessage(ct));
      }
    }
  );
  
  // Simple ping messages
  m_rpc.registerMethod<Protocol::PingRequest, Protocol::PingResponse>("Core.Ping",
    [this](const Protocol::PingRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::PingResponse> {
      // Add peer to routing table when message has been delivered directly
      if (msg.originLinkId() == msg.sourceNodeId())
        m_routes.add(m_manager.getLinkContact(msg.originLinkId()));
      
      Protocol::PingResponse response;
      response.set_timestamp(1);
      return RpcResponse<Protocol::PingResponse>(
        response,
        RoutingOptions().setDeliverVia(msg.sourceNodeId())
      );
    }
  );
}

void Router::leave()
{
  RecursiveUniqueLock lock(m_mutex);
  
  if (m_state != State::Joined)
    return;
  
  // Switch to leaving state and reconnect the rejoin signal
  m_state = State::Leaving;
  boost::signals2::scoped_connection *connection = new boost::signals2::scoped_connection;
  m_routes.signalRejoin.disconnect(boost::bind(&Router::join, this));
  *connection = m_routes.signalRejoin.connect([connection, this]{
    UNISPHERE_LOG(m_manager, Info, "Router: Left the overlay.");
    
    // When the routing table is empty, switch to init state
    m_state = State::Init;
    delete connection;
  });
  
  // Notify all nodes that we are leaving
  for (NodeIdentifier nodeId : m_manager.getLinkIds()) {
    m_rpc.call<Protocol::LeaveNodeRequest>(nodeId, "Core.LeaveNode");
  }
}

void Router::join()
{
  RecursiveUniqueLock lock(m_mutex);
  
  if (m_state == State::Leaving)
    return;
  
  Contact bootstrapContact = m_bootstrap.getBootstrapContact();
  m_bootstrap.signalContactReady.disconnect(boost::bind(&Router::join, this));
  if (bootstrapContact.isNull()) {
    // Bootstrap contact is not yet read, we should be called again when one becomes available
    m_bootstrap.signalContactReady.connect(boost::bind(&Router::join, this));
    return;
  }
  
  UNISPHERE_LOG(m_manager, Info, "Router: Joining the overlay network.");
  
  m_state = State::Bootstrap;
  m_routes.add(bootstrapContact);
  m_pendingContacts.clear();
  
  using namespace Protocol;
  FindNodeRequest request;
  request.set_num_contacts(m_routes.maxSiblingsSize());
  *request.mutable_local_contact() = m_manager.getLocalContact().toMessage();
  
  // Route a discovery message to our own identifier
  m_rpc.call<FindNodeRequest, FindNodeResponse>(m_manager.getLocalNodeId(), "Core.FindNode", request,
    // Success handler
    [this](const FindNodeResponse &response, const UniSphere::RoutedMessage &msg) {
      // Check for identifier collisions (unlikely but could happen)
      BOOST_ASSERT(msg.sourceNodeId() != m_manager.getLocalNodeId());
      
      // Contact returned neighbours
      for (const Protocol::Contact &ct : response.contacts()) {
        pingContact(UniSphere::Contact::fromMessage(ct));
      }
      
      // Contact all the rest that we have got to know in the join process
      {
        RecursiveUniqueLock lock(m_mutex);
        for (const UniSphere::Contact &ct : m_pendingContacts) {
          pingContact(ct);
        }
        m_pendingContacts.clear();
      }
      
      UNISPHERE_LOG(m_manager, Info, "Router: Successfully joined the overlay.");
      
      // We are now in the "joined" state
      m_state = State::Joined;
      m_routes.signalRejoin.disconnect(boost::bind(&Router::join, this));
      m_routes.signalRejoin.connect(boost::bind(&Router::join, this));
      
      // Notify subscribers
      signalJoined();
    },
    // Error handler
    [this](RpcErrorCode, const std::string&) {
      UNISPHERE_LOG(m_manager, Error, "Router: Failed to bootstrap!");
      join();
    },
    // Options
    RpcCallOptions().setDeliverVia(bootstrapContact)
  );
}

void Router::create()
{
  if (m_state != State::Init)
    return;
  
  UNISPHERE_LOG(m_manager, Info, "Router: Creating the overlay network.");
  
  m_state = State::Joined;
  signalJoined();
}

void Router::pingContact(const Contact &contact)
{
  // Only ping the contact if it is non-routable
  if (m_routes.get(contact.nodeId()).isValid())
    return;
  
  using namespace Protocol;
  PingRequest request;
  request.set_timestamp(0);
  
  m_rpc.call<PingRequest, PingResponse>(contact.nodeId(), "Core.Ping", request,
    // Success handler
    [this, contact](const PingResponse&, const UniSphere::RoutedMessage&) {
      m_routes.add(contact);
    },
    // Error handler
    nullptr,
    // Options
    RpcCallOptions().setDeliverVia(contact)
  );
}

void Router::messageReceived(const Message &msg)
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
    UNISPHERE_LOG(m_manager, Warning, "Router: Dropping invalid message.");
    UNISPHERE_MEASURE_INC(m_manager, "messages.dropped");
    return;
  }
  
  // Routing options can override the next hop
  if (!msg.options().deliverVia.isNull()) {
    m_manager.send(msg.options().deliverVia, Message(Message::Type::Plexus_Routed, *msg.serialize()));
    UNISPHERE_MEASURE_INC(m_manager, "messages.forward");
    return;
  }
  
  // Determine the next hop we will use for forwarding the message
  DistanceOrderedTable nextHops = m_routes.lookup(msg.destinationKeyId(), Router::bucket_size);
  PeerEntry nextHop;
  for (const PeerEntry &entry : nextHops.table().get<DistanceTag>()) {
    if (entry.nodeId == msg.originLinkId())
      continue;
    else if (entry.nodeId == m_manager.getLocalNodeId() &&
             !m_routes.isSiblingFor(m_manager.getLocalNodeId(), msg.destinationKeyId()))
      continue;
    
    nextHop = entry;
    break;
  }
  
  if (nextHop.isNull()) {
    UNISPHERE_LOG(m_manager, Warning, "Router: No route to destination.");
    UNISPHERE_MEASURE_INC(m_manager, "messages.dropped");
    return;
  }
  
  // Check if the message is destined to the local node, in this case it should be
  // delivered to an upper layer application/component
  if (nextHop.nodeId == m_manager.getLocalNodeId()) {
    BOOST_ASSERT(!msg.originLinkId().isNull());
    UNISPHERE_MEASURE_INC(m_manager, "messages.local");
    signalDeliverMessage(msg);
    return;
  } else {
    UNISPHERE_MEASURE_INC(m_manager, "messages.forward");
    signalForwardMessage(msg);
    m_manager.send(nextHop.contact, Message(Message::Type::Plexus_Routed, *msg.serialize()));
  }
}

void Router::route(std::uint32_t sourceCompId, const NodeIdentifier &key,
                   std::uint32_t destinationCompId, std::uint32_t type,
                   const google::protobuf::Message &msg,
                   const RoutingOptions &opts)
{
  // First encapsulate the specified application message into a routed message
  RoutedMessage rmsg(m_manager.getLocalNodeId(), sourceCompId, key, destinationCompId, type, msg, opts);
  // Attempt to route the generated message
  route(rmsg);
}


}
