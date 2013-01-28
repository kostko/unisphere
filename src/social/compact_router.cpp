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
#include "src/social/messages.pb.h"
#include "core/operators.h"

namespace UniSphere {

CompactRouter::CompactRouter(SocialIdentity &identity, LinkManager &manager,
                             NetworkSizeEstimator &sizeEstimator)
  : m_context(manager.context()),
    m_identity(identity),
    m_manager(manager),
    m_sizeEstimator(sizeEstimator),
    m_routes(m_context, identity.localId(), sizeEstimator),
    m_announceTimer(manager.context().service())
{
  BOOST_ASSERT(identity.localId() == manager.getLocalNodeId());
}

void CompactRouter::initialize()
{
  UNISPHERE_LOG(m_manager, Info, "CompactRouter: Initializing router.");

  // Subscribe to all events
  m_subscriptions
    << m_manager.signalMessageReceived.connect(boost::bind(&CompactRouter::messageReceived, this, _1))
    << m_manager.signalVerifyPeer.connect(boost::bind(&CompactRouter::linkVerifyPeer, this, _1))
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&CompactRouter::networkSizeEstimateChanged, this, _1))
    << m_routes.signalExportEntry.connect(boost::bind(&CompactRouter::ribExportEntry, this, _1))
    << m_routes.signalRetractEntry.connect(boost::bind(&CompactRouter::ribRetractEntry, this, _1))
    << m_identity.signalPeerAdded.connect(boost::bind(&CompactRouter::peerAdded, this, _1))
    << m_identity.signalPeerRemoved.connect(boost::bind(&CompactRouter::peerRemoved, this, _1))
  ;

  // Compute whether we should become a landmark or not
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start announcing ourselves to all neighbours
  announceOurselves(boost::system::error_code());
}

void CompactRouter::shutdown()
{
  UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Shutting down router.");

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

  // Announce ourselves to all neighbours
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

    // Send the announcement
    m_manager.send(peer.second, Message(Message::Type::Social_Announce, announce));
  }

  // Redo announce after 10 seconds
  m_announceTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_announce));
  m_announceTimer.async_wait(boost::bind(&CompactRouter::announceOurselves, this, _1));
}

void CompactRouter::ribExportEntry(const RoutingEntry &entry)
{
  // Export entry to all neighbors
  Protocol::PathAnnounce announce;
  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    // Retrieve vport for given peer and check that it is not the origin
    Vport vport = m_routes.getVportForNeighbor(peer.first);
    if (vport == entry.originVport())
      continue;

    // Prepare the announce message
    announce.Clear();
    announce.set_destinationid(entry.destination.as(NodeIdentifier::Format::Raw));
    announce.set_type(static_cast<Protocol::PathAnnounce_Type>(entry.type));

    for (Vport v : entry.forwardPath) {
      announce.add_forwardpath(v);
    }

    for (Vport v : entry.reversePath) {
      announce.add_reversepath(v);
    }
    announce.add_reversepath(vport);

    // Send the announcement
    m_manager.send(peer.second, Message(Message::Type::Social_Announce, announce));
  }

  // TODO: Think about compaction/aggregation of multiple entries
}

void CompactRouter::ribRetractEntry(const RoutingEntry &entry)
{
  // Send retraction message to all neighbors
  Protocol::PathRetract retract;
  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    // Retrieve vport for given peer and check that it is not the origin
    Vport vport = m_routes.getVportForNeighbor(peer.first);
    if (vport == entry.originVport())
      continue;

    // Prepare the retract message
    retract.Clear();
    retract.set_destinationid(entry.destination.as(NodeIdentifier::Format::Raw));

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
      RoutingEntry entry(
        NodeIdentifier(pan.destinationid(), NodeIdentifier::Format::Raw),
        static_cast<RoutingEntry::Type>(pan.type())
      );

      // Get the incoming vport for this announcement; if none is available a
      // new vport is automatically assigned
      Vport vport = m_routes.getVportForNeighbor(msg.originator());
      entry.forwardPath.push_back(vport);

      // Populate the forwarding path
      for (int i = 0; i < pan.forwardpath_size(); i++) {
        entry.forwardPath.push_back(pan.forwardpath(i));
      }

      // Populate the reverse path (for landmarks)
      if (entry.type == RoutingEntry::Type::Landmark) {
        for (int i = 0; i < pan.reversepath_size(); i++) {
          entry.reversePath.push_back(pan.reversepath(i));
        }
      }

      // Compute cost based on hop count and set entry timestamp
      entry.cost = entry.forwardPath.size();
      entry.lastUpdate = boost::posix_time::microsec_clock::universal_time();

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
  
}
