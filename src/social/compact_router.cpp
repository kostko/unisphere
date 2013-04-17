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
#include "social/social_identity.h"
#include "social/routing_table.h"
#include "social/name_database.h"
#include "social/rpc_engine.h"
#include "social/sloppy_group.h"
#include "interplex/link_manager.h"
#include "core/operators.h"

#include "src/social/messages.pb.h"
#include "src/social/core_methods.pb.h"

namespace UniSphere {

/**
 * This structure is used for each peer to manage aggregation of RIB
 * exports. Multiple announces are buffered and then transmitted in
 * a single message to reduce the number of messages.
 */
struct AggregationBuffer {
  /**
   * Class constructor.
   */
  AggregationBuffer(Context &context, const Contact &contact)
    : contact(contact),
      timer(context.service()),
      buffering(false)
  {}

  /// Contact we are agregating for
  Contact contact;
  /// Aggregated path announcement
  Protocol::AggregatePathAnnounce aggregate;
  /// Timer to transmit the buffered announcement
  boost::asio::deadline_timer timer;
  /// Buffering indicator
  bool buffering;
};

UNISPHERE_SHARED_POINTER(AggregationBuffer);

class CompactRouterPrivate {
public:
  /**
   * Class constructor.
   */
  CompactRouterPrivate(SocialIdentity &identity,
                       LinkManager &manager,
                       NetworkSizeEstimator &sizeEstimator,
                       CompactRouter &router);

  /**
   * Initializes the router.
   */
  void initialize();

  /**
   * Shuts down the router and all components.
   */
  void shutdown();

  /**
   * Routes the specified message via the overlay.
   *
   * @param msg A valid message to be routed
   */
  void route(RoutedMessage &msg);

  /**
   * Called when a message has been received on any link.
   *
   * @param msg Link-local message that has been received
   */
  void messageReceived(const Message &msg);

  /**
   * Called when the network size estimate is changed.
   *
   * @param size New network size estimate
   */
  void networkSizeEstimateChanged(std::uint64_t size);

  /**
   * Announces the current node to all neighbors.
   */
  void announceOurselves(const boost::system::error_code &error);

  /**
   * Request full routes from all neighbors.
   */
  void requestFullRoutes();

  /**
   * Called when a new peer is added to the social identity.
   */
  void peerAdded(const Contact &peer);

  /**
   * Called when a peer is removed from the social identity.
   */
  void peerRemoved(const NodeIdentifier &nodeId);

  /**
   * A handler for verification of peer contacts.
   *
   * @param peer Peer contact to verify
   * @return True if verification is successful, false otherwise
   */
  bool linkVerifyPeer(const Contact &peer);

  /**
   * Called by the routing table when an entry should be exported to
   * all neighbors.
   *
   * @param entry Routing entry to export
   * @param peer Optional peer identifier to export to
   */
  void ribExportEntry(RoutingEntryPtr entry,
                      const NodeIdentifier &peer = NodeIdentifier::INVALID);

  /**
   * Queues an announce for transmission.
   *
   * @param contact Peer to transmit the announce to
   * @param announce Announce message
   */
  void ribExportQueueAnnounce(const Contact &contact,
                              const Protocol::PathAnnounce &announce);

  /**
   * Transmits buffered RIB export messages.
   *
   * @param error Error code
   * @param buffer Aggregation buffer that should be transmitted
   */
  void ribExportTransmitBuffer(const boost::system::error_code &error,
                               AggregationBufferPtr buffer);

  /**
   * Called by the routing table when a retraction should be sent to
   * all neighbors.
   *
   * @param entry Routing entry to retract
   */
  void ribRetractEntry(RoutingEntryPtr entry);

  /**
   * Called when a new landmark is learned via route exchange.
   *
   * @param landmarkId Landmark identifier
   */
  void landmarkLearned(const NodeIdentifier &landmarkId);

  /**
   * Called when a landmark is unlearned.
   *
   * @param landmarkId Landmark identifier
   */
  void landmarkRemoved(const NodeIdentifier &landmarkId);

  /**
   * Performs registration of core RPC methods that are required for routing.
   */
  void registerCoreRpcMethods();

  /**
   * Performs unregistration of core RPC methods that are required for routing.
   */
  void unregisterCoreRpcMethods();
public:
  UNISPHERE_DECLARE_PUBLIC(CompactRouter)
  UNISPHERE_DECLARE_PREMATURE_PARENT_SETTER(CompactRouter)

  /// UNISPHERE context
  Context &m_context;
  /// Mutex protecting the compact router
  mutable std::recursive_mutex m_mutex;
  /// Local node identity
  SocialIdentity &m_identity;
  /// Link manager associated with this router
  LinkManager &m_manager;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Compact routing table
  CompactRoutingTable m_routes;
  /// RPC engine
  RpcEngine m_rpc;
  /// Name database
  NameDatabase m_nameDb;
  /// Sloppy group manager
  SloppyGroupManager m_sloppyGroup;
  /// Timer for notifying neighbours about ourselves
  boost::asio::deadline_timer m_announceTimer;
  /// Active subscriptions to other components
  std::list<boost::signals2::connection> m_subscriptions;
  /// Local sequence number
  std::uint16_t m_seqno;
  /// Aggregated path announcement
  std::unordered_map<NodeIdentifier, AggregationBufferPtr> m_ribExportAggregate;
};

CompactRouterPrivate::CompactRouterPrivate(SocialIdentity &identity,
                                           LinkManager &manager,
                                           NetworkSizeEstimator &sizeEstimator,
                                           CompactRouter &router)
  : q(router),
    w(router, this),
    m_context(manager.context()),
    m_identity(identity),
    m_manager(manager),
    m_sizeEstimator(sizeEstimator),
    m_routes(m_context, identity.localId(), sizeEstimator),
    m_rpc(router),
    m_nameDb(router),
    m_sloppyGroup(router, sizeEstimator),
    m_announceTimer(manager.context().service()),
    m_seqno(1)
{
  BOOST_ASSERT(identity.localId() == manager.getLocalNodeId());
}

void CompactRouterPrivate::initialize()
{
  UNISPHERE_LOG(m_manager, Info, "CompactRouter: Initializing router.");

  // Register core routing RPC methods
  registerCoreRpcMethods();

  // Subscribe to all events
  m_subscriptions
    << m_manager.signalMessageReceived.connect(boost::bind(&CompactRouterPrivate::messageReceived, this, _1))
    << m_manager.signalVerifyPeer.connect(boost::bind(&CompactRouterPrivate::linkVerifyPeer, this, _1))
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&CompactRouterPrivate::networkSizeEstimateChanged, this, _1))
    << m_routes.signalExportEntry.connect(boost::bind(&CompactRouterPrivate::ribExportEntry, this, _1, _2))
    << m_routes.signalRetractEntry.connect(boost::bind(&CompactRouterPrivate::ribRetractEntry, this, _1))
    << m_routes.signalLandmarkLearned.connect(boost::bind(&CompactRouterPrivate::landmarkLearned, this, _1))
    << m_routes.signalLandmarkRemoved.connect(boost::bind(&CompactRouterPrivate::landmarkRemoved, this, _1))
    << m_identity.signalPeerAdded.connect(boost::bind(&CompactRouterPrivate::peerAdded, this, _1))
    << m_identity.signalPeerRemoved.connect(boost::bind(&CompactRouterPrivate::peerRemoved, this, _1))
  ;

  // Compute whether we should become a landmark or not
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start announcing ourselves to all neighbours
  announceOurselves(boost::system::error_code());

  // Initialize the name database
  m_nameDb.initialize();

  // Initialize the sloppy group manager
  m_sloppyGroup.initialize();
}

void CompactRouterPrivate::shutdown()
{
  UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Shutting down router.");

  // Unregister core routing RPC methods
  unregisterCoreRpcMethods();

  // Shutdown the sloppy group manager
  m_sloppyGroup.shutdown();

  // Shutdown the name database
  m_nameDb.shutdown();

  // Unsubscribe from all events
  for (boost::signals2::connection c : m_subscriptions)
    c.disconnect();
  m_subscriptions.clear();

  // Stop announcing ourselves
  m_announceTimer.cancel();

  // Clear the routing table
  m_routes.clear();
}

void CompactRouterPrivate::peerAdded(const Contact &peer)
{
  // Export announces to new peer when it is added
  m_routes.fullUpdate(peer.nodeId());
}

void CompactRouterPrivate::peerRemoved(const NodeIdentifier &nodeId)
{
  // TODO: Update the vport mapping?
}

void CompactRouterPrivate::announceOurselves(const boost::system::error_code &error)
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
    ribExportQueueAnnounce(peer.second, announce);

    // Send full routing table to neighbor
    m_routes.fullUpdate(peer.first);
  }

  // Redo announce after 10 seconds
  m_announceTimer.expires_from_now(m_context.roughly(CompactRouter::interval_announce));
  m_announceTimer.async_wait(boost::bind(&CompactRouterPrivate::announceOurselves, this, _1));
}

void CompactRouterPrivate::requestFullRoutes()
{
  // Request full routing table dump from all neighbors
  Protocol::PathRefresh request;
  request.set_destinationid(NodeIdentifier().as(NodeIdentifier::Format::Raw));

  for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
    m_manager.send(peer.second, Message(Message::Type::Social_Refresh, request));
  }
}

void CompactRouterPrivate::ribExportEntry(RoutingEntryPtr entry, const NodeIdentifier &peer)
{
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
    Protocol::PathAnnounce announce;
    announce.set_destinationid(entry->destination.as(NodeIdentifier::Format::Raw));
    announce.set_type(static_cast<Protocol::PathAnnounce_Type>(entry->type));
    announce.set_seqno(entry->seqno);

    for (Vport v : entry->forwardPath) {
      announce.add_forwardpath(v);
    }

    for (Vport v : entry->reversePath) {
      announce.add_reversepath(v);
    }
    announce.add_reversepath(vport);
    ribExportQueueAnnounce(contact, announce);
  };

  if (peer.isNull()) {
    // Export entry to all neighbors
    for (const std::pair<NodeIdentifier, Contact> &peer : m_identity.peers()) {
      exportEntry(peer.second);
    }
  } else {
    exportEntry(m_identity.getPeerContact(peer));
  }
}

void CompactRouterPrivate::ribExportQueueAnnounce(const Contact &contact,
                                                  const Protocol::PathAnnounce &announce)
{
  RecursiveUniqueLock lock(m_mutex);

  // Perform entry aggregation into a single announce message containing multiple
  // path announces
  AggregationBufferPtr buffer;
  auto it = m_ribExportAggregate.find(contact.nodeId());
  if (it == m_ribExportAggregate.end()) {
    buffer = AggregationBufferPtr(new AggregationBuffer(m_context, contact));
    m_ribExportAggregate.insert({{ contact.nodeId(), buffer }});
  } else {
    buffer = it->second;
  }

  Protocol::PathAnnounce *a = buffer->aggregate.add_announces();
  *a = announce;

  // Buffer further messages for another 5 seconds, then transmit all of them
  if (!buffer->buffering) {
    buffer->buffering = true;
    buffer->timer.expires_from_now(m_context.roughly(5));
    buffer->timer.async_wait(boost::bind(&CompactRouterPrivate::ribExportTransmitBuffer, this, _1, buffer));
  }
}

void CompactRouterPrivate::ribExportTransmitBuffer(const boost::system::error_code &error,
                                                   AggregationBufferPtr buffer)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  m_manager.send(
    buffer->contact,
    Message(Message::Type::Social_Announce, buffer->aggregate)
  );

  // Clear the buffer after transmission
  buffer->buffering = false;
  buffer->aggregate.Clear();
}

void CompactRouterPrivate::ribRetractEntry(RoutingEntryPtr entry)
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

  // TODO: Think about compaction/aggregation of multiple entries (perhaps retractions should be
  //       unified with announcements)
}

bool CompactRouterPrivate::linkVerifyPeer(const Contact &peer)
{
  // Refuse to establish connections with unknown peers
  if (!m_identity.isPeer(peer)) {
    UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Refusing connection with unknown peer.");
    return false;
  }

  return true;
}

void CompactRouterPrivate::messageReceived(const Message &msg)
{
  switch (msg.type()) {
    case Message::Type::Social_Announce: {
      Protocol::AggregatePathAnnounce agg = message_cast<Protocol::AggregatePathAnnounce>(msg);
      // Get the incoming vport for these announcements; if none is available a
      // new vport is automatically assigned
      Vport vport = m_routes.getVportForNeighbor(msg.originator());

      for (int j = 0; j < agg.announces_size(); j++) {
        const Protocol::PathAnnounce &pan = agg.announces(j);
        RoutingEntryPtr entry(new RoutingEntry(
          m_context.service(),
          NodeIdentifier(pan.destinationid(), NodeIdentifier::Format::Raw),
          static_cast<RoutingEntry::Type>(pan.type()),
          pan.seqno()
        ));

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

        // Attempt to import the entry into the routing table
        m_routes.import(entry);
      }
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

void CompactRouterPrivate::networkSizeEstimateChanged(std::uint64_t size)
{
  // Re-evaluate network size and check if we should alter our landmark status
  double x = std::generate_canonical<double, 32>(m_manager.context().basicRng());
  double n = static_cast<double>(size);

  // TODO: Only flip landmark status if size has changed by at least a factor 2
  if (x < std::sqrt(std::log(n) / n)) {
    UNISPHERE_LOG(m_manager, Info, "CompactRouter: Becoming a LANDMARK.");
    m_routes.setLandmark(true);
    m_nameDb.registerLandmark(m_manager.getLocalNodeId());
    // TODO: Unregister landmark when local node ceases to be one
  }
}

void CompactRouterPrivate::route(RoutedMessage &msg)
{
  // Drop invalid messages
  if (!msg.isValid()) {
    UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Dropping invalid message.");
    return;
  }

  // Check if we are the destination - deliver to local component
  if (msg.destinationNodeId() == m_manager.getLocalNodeId()) {
    // If this is a packet that has been sent to ourselves, we should dispatch the signal
    // via the event queue and not call the signal directly
    if (msg.originLinkId().isNull()) {
      m_context.service().post([this, msg]() { q.signalDeliverMessage(msg); });
    } else {
      // Cache source address when one is available
      if (!msg.sourceAddress().isNull())
        m_nameDb.store(msg.sourceNodeId(), msg.sourceAddress(), NameRecord::Type::Cache);

      q.signalDeliverMessage(msg);
    }
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
      if (msg.destinationAddress().landmarkId() == m_manager.getLocalNodeId()) {
        if (msg.destinationAddress().path().empty()) {
          // Landmark-relative address is empty but we are the designated landmark; this
          // means that we are responsible for resolving the destination L-R address
          NameRecordPtr record = m_nameDb.lookup(msg.destinationNodeId());
          if (record)
            msg.setDestinationAddress(record->landmarkAddress());
        } else {
          // We are the landmark, enter delivery mode
          msg.setDeliveryMode(true);
        }
      }

      if (msg.deliveryMode()) {
        // We must route based on source path
        if (msg.destinationAddress().path().empty()) {
          UNISPHERE_LOG(m_manager, Warning, "CompactRouter: Dropping message with dm = true and empty path.");
          return;
        }

        nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(msg.destinationAddress().path().front()));
        msg.processSourceRouteHop();
      } else {
        // We must route to landmark node
        RoutingEntryPtr entry = m_routes.getActiveRoute(msg.destinationAddress().landmarkId());
        if (entry)
          nextHop = m_identity.getPeerContact(m_routes.getNeighborForVport(entry->originVport()));
      }
    }

    if (nextHop.isNull()) {
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
  q.signalForwardMessage(msg);
  Protocol::RoutedMessage pmsg;
  msg.serialize(pmsg);
  m_manager.send(nextHop, Message(Message::Type::Social_Routed, pmsg));
}

void CompactRouterPrivate::landmarkLearned(const NodeIdentifier &landmarkId)
{
  m_nameDb.registerLandmark(landmarkId);
}

void CompactRouterPrivate::landmarkRemoved(const NodeIdentifier &landmarkId)
{
  m_nameDb.unregisterLandmark(landmarkId);
}

void CompactRouterPrivate::registerCoreRpcMethods()
{
  // Simple ping messages
  m_rpc.registerMethod<Protocol::PingRequest, Protocol::PingResponse>("Core.Ping",
    [this](const Protocol::PingRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::PingResponse> {
      // Fix a hop limit so these messages can be used to measure the number of hops
      // they have traversed
      int hopCount = 30;
      
      Protocol::PingResponse response;
      response.set_timestamp(1);
      response.set_hopcount(hopCount);
      return RpcResponse<Protocol::PingResponse>(
        response,
        RoutingOptions().setHopLimit(hopCount)
      );
    }
  );
}

void CompactRouterPrivate::unregisterCoreRpcMethods()
{
  m_rpc.unregisterMethod("Core.Ping");
}

CompactRouter::CompactRouter(SocialIdentity &identity,
                             LinkManager &manager,
                             NetworkSizeEstimator &sizeEstimator)
{
  // Must be constructed in this way and will initialize the d-ptr
  new CompactRouterPrivate(identity, manager, sizeEstimator, *this);
}

void CompactRouter::initialize()
{
  d->initialize();
}

void CompactRouter::shutdown()
{
  d->shutdown();
}

Context &CompactRouter::context() const
{
  return d->m_context;
}

const SocialIdentity &CompactRouter::identity() const
{
  return d->m_identity;
}

const LinkManager &CompactRouter::linkManager() const
{
  return d->m_manager;
}

CompactRoutingTable &CompactRouter::routingTable()
{
  return d->m_routes;
}

NameDatabase &CompactRouter::nameDb()
{
  return d->m_nameDb;
}

SloppyGroupManager &CompactRouter::sloppyGroup()
{
  return d->m_sloppyGroup;
}

RpcEngine &CompactRouter::rpcEngine()
{
  return d->m_rpc;
}

void CompactRouter::route(RoutedMessage &msg)
{
  d->route(msg);
}

void CompactRouter::route(std::uint32_t sourceCompId,
                          const NodeIdentifier &destination,
                          const LandmarkAddress &destinationAddress,
                          std::uint32_t destinationCompId,
                          std::uint32_t type,
                          const google::protobuf::Message &msg,
                          const RoutingOptions &opts)
{
  // Create a new routed message without a known destination L-R address
  RoutedMessage rmsg(
    d->m_routes.getLocalAddress(),
    d->m_manager.getLocalNodeId(),
    sourceCompId,
    destinationAddress,
    destination,
    destinationCompId,
    type,
    msg,
    opts
  );

  // Processing of locally originating messages should be deferred to avoid deadlocks
  d->m_context.service().post(boost::bind(&CompactRouterPrivate::route, d, rmsg));
}

}
