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
    m_pendingContactTimer(manager.context().service()),
    m_state(State::Init)
{
  m_manager.setLinkInitializer(boost::bind(&Router::initializeLink, this, _1));
  
  // Setup the periodic contact establishment timer
  m_pendingContactTimer.expires_from_now(boost::posix_time::seconds(Router::pending_contacts_period));
  m_pendingContactTimer.async_wait(boost::bind(&Router::connectToMoreContacts, this));
  
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
          *contact = entry.contact().toMessage();
      }
      
      Contact backContact = Contact::fromMessage(request.local_contact());
      return RpcResponse<Protocol::FindNodeResponse>(
        response,
        RoutingOptions().setDeliverVia(m_manager.connect(backContact))
      );
    }
  );
  
  // Leave node RPC that signals when a node is leaving
  m_rpc.registerMethod<Protocol::LeaveNodeRequest>("Core.LeaveNode",
    [this](const Protocol::LeaveNodeRequest&, const RoutedMessage &msg, RpcId) {
      UNISPHERE_LOG(m_manager, Info, "Node " + msg.sourceNodeId().as(NodeIdentifier::Format::Hex) + " is leaving.");
      
      // Terminate link with node and remove it from the routing tables
      LinkPtr link = m_manager.getLink(msg.sourceNodeId());
      m_routes.remove(msg.sourceNodeId());
      if (link)
        link->close();
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
          *contact = entry.contact().toMessage();
      }
      
      // Perform a back-request without confirmation to the source node
      Contact backContact = Contact::fromMessage(request.local_contact());
      m_rpc.call<Protocol::ExchangeEntriesRequest>(msg.sourceNodeId(), "Core.ExchangeEntries", backRequest,
        RpcCallOptions().setDeliverVia(m_manager.connect(backContact))
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
      
      // Queue all contacts to be contacted
      for (const Protocol::Contact &ct : request.contacts()) {
        queueContact(Contact::fromMessage(ct));
      }
    }
  );
  
  // Simple ping messages
  m_rpc.registerMethod<Protocol::PingRequest, Protocol::PingResponse>("Core.Ping",
    [this](const Protocol::PingRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::PingResponse> {
      Protocol::PingResponse rsp;
      rsp.set_timestamp(1);
      return rsp;
    }
  );
}

void Router::queueContact(const Contact &contact)
{
  RecursiveUniqueLock lock(m_mutex);
  if (m_pendingContacts.get<1>().find(contact.nodeId()) != m_pendingContacts.get<1>().end())
    return;
  
  if (m_pendingContacts.size() >= Router::pending_contacts_size)
    return;
  
  m_pendingContacts.push_front(contact);
}

void Router::connectToMoreContacts()
{
  RecursiveUniqueLock lock(m_mutex);
  
  // Retrieve the last contact and attempt to establish contact
  if (m_pendingContacts.size() > 0) {
    m_manager.connect(m_pendingContacts.back());
    m_pendingContacts.pop_back();
  }
  
  // Setup the periodic contact establishment timer
  m_pendingContactTimer.expires_from_now(boost::posix_time::seconds(Router::pending_contacts_period));
  m_pendingContactTimer.async_wait(boost::bind(&Router::connectToMoreContacts, this));
}

void Router::leave()
{
  if (m_state != State::Joined)
    return;
  
  // Switch to leaving state and reconnect the rejoin signal
  m_state = State::Leaving;
  boost::signals::scoped_connection *connection = new boost::signals::scoped_connection;
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
  
  m_state = State::Init;
  LinkPtr link = m_manager.connect(bootstrapContact);
  link->callWhenConnected(boost::bind(&Router::bootstrapStart, this, _1));
}

void Router::create()
{
  if (m_state != State::Init)
    return;
  
  UNISPHERE_LOG(m_manager, Info, "Router: Creating the overlay network.");
  
  m_state = State::Joined;
  signalJoined();
}

void Router::bootstrapStart(LinkPtr link)
{
  // Perform bootstrap lookup procedure when we are in init state
  RecursiveUniqueLock lock(m_mutex);
  if (m_state == State::Init) {
    using namespace Protocol;
    FindNodeRequest request;
    request.set_num_contacts(m_routes.maxSiblingsSize());
    *request.mutable_local_contact() = m_manager.getLocalContact().toMessage();
    
    UNISPHERE_LOG(m_manager, Info, "Router: Entering bootstrap phase.");
    
    // The first phase is to contact the bootstrap node to give us contact information
    m_state = State::Bootstrap;
    m_rpc.call<FindNodeRequest, FindNodeResponse>(m_manager.getLocalNodeId(), "Core.FindNode", request,
      // Success handler
      [this](const FindNodeResponse &response, const UniSphere::RoutedMessage &msg) {
        // Check for identifier collisions (unlikely but could happen)
        BOOST_ASSERT(msg.sourceNodeId() != m_manager.getLocalNodeId());
        
        // Queue all contacts to be contacted
        for (const Protocol::Contact &ct : response.contacts()) {
          queueContact(UniSphere::Contact::fromMessage(ct));
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
      [this, link](RpcErrorCode, const std::string&) {
        UNISPHERE_LOG(m_manager, Error, "Router: Failed to bootstrap!");
        
        link->close();
        join();
      },
      // Options
      RpcCallOptions().setDeliverVia(link)
    );
  }
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
  if (!m_routes.add(link)) {
    UNISPHERE_LOG(m_manager, Warning, "Router: Unable to add established link to table!");
    
    link->close();
    return;
  }
}

void Router::linkLost(LinkPtr link)
{
  // Removes the lost link from the routing table
  m_routes.remove(link);
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
    UNISPHERE_LOG(m_manager, Warning, "Router: Dropping invalid message.");
    return;
  }
  
  // Routing options can override the next hop
  if (LinkPtr deliverVia = msg.options().deliverVia.lock()) {
    deliverVia->send(Message(Message::Type::Plexus_Routed, *msg.serialize()));
    return;
  }
  
  // Determine the next hop we will use for forwarding the message
  DistanceOrderedTable nextHops = m_routes.lookup(msg.destinationKeyId(), Router::bucket_size);
  PeerEntry nextHop;
  for (const PeerEntry &entry : nextHops.table().get<DistanceTag>()) {
    if (entry.link == msg.originator().lock())
      continue;
    else if (entry.nodeId == m_manager.getLocalNodeId() &&
             !m_routes.isSiblingFor(m_manager.getLocalNodeId(), msg.destinationKeyId()))
      continue;
    
    nextHop = entry;
    break;
  }
  
  if (nextHop.isNull()) {
    UNISPHERE_LOG(m_manager, Warning, "Router: No route to destination.");
    return;
  }
  
  // Check if the message is destined to the local node, in this case it should be
  // delivered to an upper layer application/component
  if (nextHop.nodeId == m_manager.getLocalNodeId()) {
    UNISPHERE_LOG(m_manager, Info, "Received local message.");
    signalDeliverMessage(msg);
    return;
  } else {
    signalForwardMessage(msg);
    nextHop.link->send(Message(Message::Type::Plexus_Routed, *msg.serialize()));
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
