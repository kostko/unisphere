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
#include "social/compact_router.h"
#include "social/size_estimator.h"
#include "interplex/link_manager.h"
#include "core/operators.h"

#include "src/social/messages.pb.h"
#include "src/social/core_methods.pb.h"


namespace UniSphere {

CompactRouter::CompactRouter(SocialIdentity &identity, LinkManager &manager,
                             NetworkSizeEstimator &sizeEstimator)
  : m_context(manager.context()),
    m_identity(identity),
    m_manager(manager),
    m_sizeEstimator(sizeEstimator),
    m_routes(m_context, identity.localId(), sizeEstimator),
    m_nameDb(*this),
    m_rpc(*this),
    m_announceTimer(manager.context().service()),
    m_seqno(1)
{
  BOOST_ASSERT(identity.localId() == manager.getLocalNodeId());
}

void CompactRouter::initialize()
{
  UNISPHERE_LOG(m_manager, Info, "CompactRouter: Initializing router.");

  // Register core routing RPC methods
  registerCoreRpcMethods();

  // Subscribe to all events
  m_subscriptions
    << m_manager.signalMessageReceived.connect(boost::bind(&CompactRouter::messageReceived, this, _1))
    << m_manager.signalVerifyPeer.connect(boost::bind(&CompactRouter::linkVerifyPeer, this, _1))
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&CompactRouter::networkSizeEstimateChanged, this, _1))
    << m_routes.signalExportEntry.connect(boost::bind(&CompactRouter::ribExportEntry, this, _1, _2))
    << m_routes.signalRetractEntry.connect(boost::bind(&CompactRouter::ribRetractEntry, this, _1))
    << m_routes.signalLandmarkLearned.connect(boost::bind(&CompactRouter::landmarkLearned, this, _1))
    << m_routes.signalLandmarkRemoved.connect(boost::bind(&CompactRouter::landmarkRemoved, this, _1))
    << m_identity.signalPeerAdded.connect(boost::bind(&CompactRouter::peerAdded, this, _1))
    << m_identity.signalPeerRemoved.connect(boost::bind(&CompactRouter::peerRemoved, this, _1))
  ;

  // Compute whether we should become a landmark or not
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start announcing ourselves to all neighbours
  announceOurselves(boost::system::error_code());

  // Initialize the name database
  m_nameDb.initialize();
}

void CompactRouter::shutdown()
{
  UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Shutting down router.");

  // Unregister core routing RPC methods
  unregisterCoreRpcMethods();

  // Shutdown the name database
  m_nameDb.shutdown();

  // Unsubscribe from all events
  for (boost::signals::connection c : m_subscriptions)
    c.disconnect();
  m_subscriptions.clear();

  // Stop announcing ourselves
  m_announceTimer.cancel();

  // Clear the routing table
  m_routes.clear();
}

void CompactRouter::peerAdded(const Contact &peer)
{
  // TODO: Export announces to new peer
}

void CompactRouter::peerRemoved(const NodeIdentifier &nodeId)
{
  // TODO: Update the vport mapping?
}

void CompactRouter::announceOurselves(const boost::system::error_code &error)
{
  if (error)
    return;

  // Announce ourselves to all neighbours and send them routing updates
  Protocol::PathAnnounce announce;
  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    announce.Clear();
    announce.set_destinationid(m_identity.localId().as(NodeIdentifier::Format::Raw));
    if (m_routes.isLandmark()) {
      announce.set_type(Protocol::PathAnnounce::LANDMARK);

      // Get/assign the outgoing vport for this announcement
      Vport vport = m_routes.getVportForNeighbor(peer.first);
      announce.add_reversepath(vport);
    } else {
      announce.set_type(Protocol::PathAnnounce::VICINITY);
    }

    announce.set_seqno(m_seqno);

    // Send the announcement
    m_manager.send(peer.second, Message(Message::Type::Social_Announce, announce));

    // Send full routing table to neighbor
    m_routes.fullUpdate(peer.first);
  }

  // Redo announce after 10 seconds
  m_announceTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_announce));
  m_announceTimer.async_wait(boost::bind(&CompactRouter::announceOurselves, this, _1));
}

void CompactRouter::requestFullRoutes()
{
  // Request full routing table dump from all neighbors
  Protocol::PathRefresh request;
  request.set_destinationid(NodeIdentifier().as(NodeIdentifier::Format::Raw));

  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    m_manager.send(peer.second, Message(Message::Type::Social_Refresh, request));
  }
}

void CompactRouter::ribExportEntry(RoutingEntryPtr entry, const NodeIdentifier &peer)
{
  Protocol::PathAnnounce announce;
  auto exportEntry = [&](const Contact &contact) {
    if (contact.isNull()) {
      UNISPHERE_LOG(m_manager, Error, "CompactRouter: Attempted export for null contact!");
      return;
    }

    // Retrieve vport for given peer and check that it is not the origin
    Vport vport = m_routes.getVportForNeighbor(contact.nodeId());
    if (vport == entry->originVport())
      return;

    // Prepare the announce message
    announce.Clear();
    announce.set_destinationid(entry->destination.as(NodeIdentifier::Format::Raw));
    announce.set_type(static_cast<Protocol::PathAnnounce_Type>(entry->type));

    for (Vport v : entry->forwardPath) {
      announce.add_forwardpath(v);
    }

    for (Vport v : entry->reversePath) {
      announce.add_reversepath(v);
    }
    announce.add_reversepath(vport);

    announce.set_seqno(m_seqno);

    // Send the announcement
    m_manager.send(contact, Message(Message::Type::Social_Announce, announce));
  };

  if (peer.isNull()) {
    // Export entry to all neighbors
    for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
      exportEntry(peer.second);
    }
  } else {
    exportEntry(m_identity.getPeerContact(peer));
  }

  // TODO: Think about compaction/aggregation of multiple entries
}

void CompactRouter::ribRetractEntry(RoutingEntryPtr entry)
{
  // Send retraction message to all neighbors
  Protocol::PathRetract retract;
  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    // Retrieve vport for given peer and check that it is not the origin
    Vport vport = m_routes.getVportForNeighbor(peer.first);
    if (vport == entry->originVport())
      continue;

    // Prepare the retract message
    retract.Clear();
    retract.set_destinationid(entry->destination.as(NodeIdentifier::Format::Raw));

    // Send the announcement
    m_manager.send(peer.second, Message(Message::Type::Social_Retract, retract));
  }

  // TODO: Think about compaction/aggregation of multiple entries
}

bool CompactRouter::linkVerifyPeer(const Contact &peer)
{
  // Refuse to establish connections with unknown peers
  if (!m_identity.isPeer(peer)) {
    UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Refusing connection with unknown peer.");
    return false;
  }

  return true;
}

void CompactRouter::messageReceived(const Message &msg)
{
  switch (msg.type()) {
    case Message::Type::Social_Announce: {
      Protocol::PathAnnounce pan = message_cast<Protocol::PathAnnounce>(msg);
      RoutingEntryPtr entry(new RoutingEntry(
        m_context.service(),
        NodeIdentifier(pan.destinationid(), NodeIdentifier::Format::Raw),
        static_cast<RoutingEntry::Type>(pan.type()),
        pan.seqno()
      ));

      // Get the incoming vport for this announcement; if none is available a
      // new vport is automatically assigned
      Vport vport = m_routes.getVportForNeighbor(msg.originator());
      entry->forwardPath.push_back(vport);

      // Populate the forwarding path
      for (int i = 0; i < pan.forwardpath_size(); i++) {
        entry->forwardPath.push_back(pan.forwardpath(i));
      }

      // Populate the reverse path (for landmarks)
      if (entry->type == RoutingEntry::Type::Landmark) {
        for (int i = 0; i < pan.reversepath_size(); i++) {
          entry->reversePath.push_back(pan.reversepath(i));
        }
      }

      // Compute cost based on hop count and set entry timestamp
      entry->cost = entry->forwardPath.size();
      entry->lastUpdate = boost::posix_time::microsec_clock::universal_time();

      // Attempt to import the entry into the routing table
      m_routes.import(entry);
      break;
    }

    case Message::Type::Social_Retract: {
      Protocol::PathRetract prt = message_cast<Protocol::PathRetract>(msg);
      Vport vport = m_routes.getVportForNeighbor(msg.originator());
      m_routes.retract(vport, NodeIdentifier(prt.destinationid(), NodeIdentifier::Format::Raw));
      break;
    }

    case Message::Type::Social_Refresh: {
      Protocol::PathRefresh prf = message_cast<Protocol::PathRefresh>(msg);
      NodeIdentifier destinationId = NodeIdentifier(prf.destinationid(), NodeIdentifier::Format::Raw);
      if (destinationId.isNull()) {
        m_routes.fullUpdate(msg.originator());
      } else {
        // TODO: Only send back routes for a specific destination
      }
      break;
    }

    case Message::Type::Social_Routed: {
      RoutedMessage rmsg(msg);
      rmsg.processHop();
      route(rmsg);
      break;
    }

    // Drop all other message types
    default: return;
  }
}

void CompactRouter::networkSizeEstimateChanged(std::uint64_t size)
{
  // Re-evaluate network size and check if we should alter our landmark status
  double x = std::generate_canonical<double, 32>(m_manager.context().basicRng());
  double n = static_cast<double>(size);

  // TODO: Only flip landmark status if size has changed by at least a factor 2
  if (x < std::sqrt(std::log(n) / n)) {
    UNISPHERE_LOG(m_manager, Info, "CompactRouter: Becoming a LANDMARK.");
    m_routes.setLandmark(true);
  }
}

void CompactRouter::route(const RoutedMessage &msg)
{
  // Drop invalid messages
  if (!msg.isValid()) {
    UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Dropping invalid message.");
    return;
  }

  // Check if we are the destination - deliver to local component
  if (msg.destinationNodeId() == m_manager.getLocalNodeId()) {
    // Cache source address when one is available
    if (!msg.sourceAddress().isNull())
      m_nameDb.store(msg.sourceNodeId(), msg.sourceAddress(), NameRecord::Type::Cache);

    UNISPHERE_LOG(m_manager, Info, "CompactRouter: Local delivery.");
    signalDeliverMessage(msg);
    return;
  }

  Contact nextHop;

  // Always attempt to first route directly without L-R addressing
  RoutingEntryPtr entry = m_routes.getActiveRoute(msg.destinationNodeId());
  if (entry)
    nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(entry->originVport()));

  if (!msg.options().directDelivery) {
    if (nextHop.isNull() && !msg.destinationAddress().isNull()) {
      // Message must first be routed to a specific landmark
      UNISPHERE_LOG(m_manager, Info, "CompactRouter: RT to L, addr known.");
      if (msg.destinationAddress().landmarkId() == m_manager.getLocalNodeId()) {
        UNISPHERE_LOG(m_manager, Info, "CompactRouter: L reached.");
        if (msg.destinationAddress().path().empty()) {
          // Landmark-relative address is empty but we are the designated landmark; this
          // means that we are responsible for resolving the destination L-R address
          NameRecordPtr record = m_nameDb.lookup(msg.destinationNodeId());
          if (record)
            msg.setDestinationAddress(record->landmarkAddress());
        } else {
          // We are the landmark, enter delivery mode
          UNISPHERE_LOG(m_manager, Info, "CompactRouter: Delivery mode entered.");
          msg.setDeliveryMode(true);
        }
      }

      if (msg.deliveryMode()) {
        // We must route based on source path
        UNISPHERE_LOG(m_manager, Info, "CompactRouter: Source-route delivery.");
        nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(msg.destinationAddress().path().front()));
      } else {
        // We must route to landmark node
        UNISPHERE_LOG(m_manager, Info, "CompactRouter: Delivery via L.");
        RoutingEntryPtr entry = m_routes.getActiveRoute(msg.destinationAddress().landmarkId());
        if (entry)
          nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(entry->originVport()));
      }
    }

    if (nextHop.isNull()) {
      UNISPHERE_LOG(m_manager, Info, "CompactRouter: No next hop and addr unknown.");
      // Check local sloppy group state to see if we have the address (landmark-relative address)
      NameRecordPtr record = m_nameDb.lookup(msg.destinationNodeId());
      if (record) {
        msg.setDestinationAddress(record->landmarkAddress());
        entry = m_routes.getActiveRoute(msg.destinationAddress().landmarkId());
        if (entry)
          nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(entry->originVport()));
      }

      if (nextHop.isNull()) {
        // Route via best sloppy group member in the vicinity (use sloppy group member as "landmark")
        UNISPHERE_LOG(m_manager, Info, "CompactRouter: Delivery via S-G.");
        entry = m_routes.getLongestPrefixMatch(msg.destinationNodeId());
        msg.setDestinationAddress(LandmarkAddress(entry->destination));
        if (entry)
          nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(entry->originVport()));
      }
    }
  }

  // Drop messages where no next hop can be determined
  if (nextHop.isNull()) {
    UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Dropping message - no route to destination.");
    return;
  }

  // Route to next hop
  signalForwardMessage(msg);
  m_manager.send(nextHop, Message(Message::Type::Social_Routed, *msg.serialize()));
}

void CompactRouter::route(std::uint32_t sourceCompId, const NodeIdentifier &destination,
                          std::uint32_t destinationCompId, std::uint32_t type,
                          const google::protobuf::Message &msg,
                          const RoutingOptions &opts)
{
  // Create a new routed message without a known destination L-R address
  RoutedMessage rmsg(
    m_routes.getLocalAddress(),
    m_manager.getLocalNodeId(),
    sourceCompId,
    LandmarkAddress(),
    destination,
    destinationCompId,
    type,
    msg,
    opts
  );

  route(rmsg);
}

void CompactRouter::landmarkLearned(const NodeIdentifier &landmarkId)
{
  m_nameDb.registerLandmark(landmarkId);
}

void CompactRouter::landmarkRemoved(const NodeIdentifier &landmarkId)
{
  m_nameDb.unregisterLandmark(landmarkId);
}

void CompactRouter::registerCoreRpcMethods()
{
  // Simple ping messages
  m_rpc.registerMethod<Protocol::PingRequest, Protocol::PingResponse>("Core.Ping",
    [this](const Protocol::PingRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::PingResponse> {
      Protocol::PingResponse response;
      response.set_timestamp(1);
      return response;
    }
  );
}

void CompactRouter::unregisterCoreRpcMethods()
{
  m_rpc.unregisterMethod("Core.Ping");
}

}
