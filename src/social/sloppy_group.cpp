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
#include "social/sloppy_group.h"
#include "social/compact_router.h"
#include "social/routing_table.h"
#include "social/address.h"
#include "social/name_database.h"
#include "social/social_identity.h"
#include "social/size_estimator.h"
#include "social/rpc_channel.h"
#include "rpc/engine.hpp"
#include "interplex/link_manager.h"
#include "core/operators.h"
#include "core/signal.h"

#include "src/social/messages.pb.h"

#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

namespace midx = boost::multi_index;

namespace UniSphere {

/**
 * Peer descriptor for nodes that are part of our sloppy group overlay.
 */
class SloppyPeer {
public:
  /**
   * Constructs a null sloppy peer.
   */
  SloppyPeer();

  /**
   * Constructs a sloppy peer with a specified node identifier and
   * with a known landmark address.
   *
   * @param nodeId Peer identifier
   * @param address L-R address of this peer
   * @param hops Hop count to this peer
   */
  SloppyPeer(const NodeIdentifier &nodeId,
             const LandmarkAddress &address,
             size_t hops);

  /**
   * Constructs a sloppy peer from a name record.
   *
   * @param record Name record pointer
   */
  explicit SloppyPeer(NameRecordPtr record);

  /**
   * Returns true if this peer is a null one.
   */
  bool isNull() const { return nodeId.isNull(); }

  /**
   * Returns the first landmark address of the peer.
   */
  LandmarkAddress landmarkAddress() const;

  /**
   * Makes this sloppy peer a null one.
   */
  void clear();
public:
  /// Sloppy peer node identifier
  NodeIdentifier nodeId;
  /// A list of L-R addresses for this sloppy peer
  LandmarkAddressList addresses;
  /// Hop count distance
  size_t hops;
};

/**
 * This structure is used for each peer to manage aggregation of NIB
 * exports. Multiple announces are buffered and then transmitted in
 * a single message to reduce the number of messages.
 */
struct NameAggregationBuffer {
  /**
   * Class constructor.
   */
  NameAggregationBuffer(Context &context, const SloppyPeer &peer)
    : peer(peer),
      timer(context.service()),
      buffering(false)
  {}

  /// Peer we are agregating for
  SloppyPeer peer;
  /// Name announcements
  std::unordered_map<std::string, Protocol::NameAnnounce> announces;
  /// Timer to transmit the buffered announcement
  boost::asio::deadline_timer timer;
  /// Buffering indicator
  bool buffering;
};

UNISPHERE_SHARED_POINTER(NameAggregationBuffer);

/// Peer view index tags
namespace PVTags {
  class NodeId;
  class HopCount;
}

/**
 * A data structure for storing sloppy group siblings.
 */
using PeerView = boost::multi_index_container<
  SloppyPeer,
  midx::indexed_by<
    // Unique index by node identifier
    midx::hashed_unique<
      midx::tag<PVTags::NodeId>,
      BOOST_MULTI_INDEX_MEMBER(SloppyPeer, NodeIdentifier, nodeId)
    >,

    // Index by hop count
    midx::ordered_non_unique<
      midx::tag<PVTags::HopCount>,
      BOOST_MULTI_INDEX_MEMBER(SloppyPeer, size_t, hops)
    >
  >
>;

/**
 * Private details of the sloppy group manager.
 */
class SloppyGroupManagerPrivate {
public:
  /**
   * Sloppy group message types.
   */
  enum class MessageType : std::uint8_t {
    /// Name announce propagated via DV-like protocol
    NameAnnounce  = 0x01,
  };

  SloppyGroupManagerPrivate(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  void initialize();

  void shutdown();

  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve);

  void dumpTopology(SloppyGroupManager::TopologyDumpGraph &graph,
                    std::function<std::string(const NodeIdentifier&)> resolve);

  /**
   * Announces local sloppy group name records to the neighbor set.
   */
  void announceFullRecords();

  void nibExportQueueRecord(const SloppyPeer &peer,
                            const Protocol::NameAnnounce &announce);

  void nibExportTransmitBuffer(const boost::system::error_code &error,
                               NameAggregationBufferPtr buffer);

  void nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId);

  void networkSizeEstimateChanged(std::uint64_t size);

  void ribVicinityLearned(const CompactRoutingTable::VicinityDescriptor &vicinity);

  /**
   * Returns the local peer view (generated from extended vicinity).
   */
  PeerView getLocalPeerView() const;

  /**
   * Refreshes the foreign peer view.
   */
  void refreshForeignPeerView();

  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const RoutedMessage &msg);
public:
  /// Router instance
  CompactRouter &m_router;
  /// Logger instance
  Logger m_logger;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Name database reference
  NameDatabase &m_nameDb;
  /// Mutex protecting the sloppy group manager
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;

  /// Foreign peer view of O(log n) entries
  PeerView m_peerViewForeign;
  /// Reverse peer view of O((log n)^2) entries
  PeerView m_peerViewReverse;

  /// Signal for announcing own records (periodic announce every 600 seconds)
  PeriodicRateDelayedSignal<5, 15, 30, 600> m_announceSignal;
  /// Signal for maintaining the sloppy group topology (periodic refresh every 600 seconds)
  PeriodicRateLimitedSignal<30, 600> m_maintenanceSignal;

  /// Aggregated name announcement
  std::unordered_map<NodeIdentifier, NameAggregationBufferPtr> m_nibExportAggregate;

  /// Active subscriptions to other components
  std::list<boost::signals2::connection> m_subscriptions;
  /// Sloppy group prefix length
  size_t m_groupPrefixLength;
  /// Sloppy group prefix
  NodeIdentifier m_groupPrefix;
  /// Expected sloppy group size
  double m_groupSize;
  /// Maximum peer view size
  size_t m_maxPeerViewSize;

  /// Statistics
  SloppyGroupManager::Statistics m_statistics;
};

SloppyPeer::SloppyPeer()
{
}

SloppyPeer::SloppyPeer(const NodeIdentifier &nodeId,
                       const LandmarkAddress &address,
                       size_t hops)
  : nodeId(nodeId),
    addresses({ address }),
    hops(hops)
{
}

SloppyPeer::SloppyPeer(NameRecordPtr record)
  : nodeId(record->nodeId),
    addresses(record->addresses)
{
}

LandmarkAddress SloppyPeer::landmarkAddress() const
{
  if (addresses.empty())
    return LandmarkAddress();

  return addresses.front();
}

void SloppyPeer::clear()
{
  nodeId = NodeIdentifier::INVALID;
  addresses.clear();
}

SloppyGroupManagerPrivate::SloppyGroupManagerPrivate(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : m_router(router),
    m_logger(logging::keywords::channel = "sloppy_group_manager"),
    m_sizeEstimator(sizeEstimator),
    m_nameDb(router.nameDb()),
    m_localId(router.identity().localId()),
    m_announceSignal(router.context()),
    m_maintenanceSignal(router.context()),
    m_groupPrefixLength(0)
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(m_localId));
}

void SloppyGroupManagerPrivate::initialize()
{
  RecursiveUniqueLock lock(m_mutex);

  BOOST_LOG_SEV(m_logger, log::normal) << "Initializing sloppy group manager.";

  m_statistics = SloppyGroupManager::Statistics();

  // Subscribe to all events
  m_subscriptions
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&SloppyGroupManagerPrivate::networkSizeEstimateChanged, this, _1))
    << m_nameDb.signalExportRecord.connect(boost::bind(&SloppyGroupManagerPrivate::nibExportRecord, this, _1, _2))
    << m_router.signalDeliverMessage.connect(boost::bind(&SloppyGroupManagerPrivate::messageDelivery, this, _1))
    << m_announceSignal.connect(boost::bind(&SloppyGroupManagerPrivate::announceFullRecords, this))
    << m_maintenanceSignal.connect(boost::bind(&SloppyGroupManagerPrivate::refreshForeignPeerView, this))
    << m_router.routingTable().signalVicinityLearned.connect(boost::bind(&SloppyGroupManagerPrivate::ribVicinityLearned, this, _1))
  ;

  // Initialize sloppy group prefix length
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start periodic signals
  m_announceSignal.start();
  m_maintenanceSignal.start();
}

void SloppyGroupManagerPrivate::shutdown()
{
  RecursiveUniqueLock lock(m_mutex);

  BOOST_LOG_SEV(m_logger, log::normal) << "Shutting down sloppy group manager.";

  m_statistics = SloppyGroupManager::Statistics();

  // Unsubscribe from all events
  for (boost::signals2::connection c : m_subscriptions)
    c.disconnect();
  m_subscriptions.clear();

  // Cancel periodic signals
  m_announceSignal.stop();
  m_maintenanceSignal.stop();

  // Clear the peer views
  m_peerViewForeign.clear();
  m_peerViewReverse.clear();
}

void SloppyGroupManagerPrivate::networkSizeEstimateChanged(std::uint64_t size)
{
  RecursiveUniqueLock lock(m_mutex);
  double n = static_cast<double>(size);
  m_groupPrefixLength = static_cast<int>(std::floor(std::log(std::sqrt(n / std::log(n))) / std::log(2.0)));
  // TODO: Only change the prefix length when n changes by at least some constant factor (eg. 10%)

  m_groupPrefix = m_localId.prefix(m_groupPrefixLength);
  m_groupSize = std::sqrt(n * std::log(n));
  m_maxPeerViewSize = static_cast<int>(std::round(std::log(n)));
  // Limit the minimum peer view size
  if (m_maxPeerViewSize < 4)
    m_maxPeerViewSize = 4;

  // Preempt peer view maintenance when the group prefix length has been changed
  m_maintenanceSignal();
}

void SloppyGroupManagerPrivate::ribVicinityLearned(const CompactRoutingTable::VicinityDescriptor &vicinity)
{
  if (vicinity.nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
    return;

  // If this node is currently in a reverse view, it should be removed from it
  // and also no full update should be sent as the node has already been updated
  // by the peer view update method
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_peerViewReverse.find(vicinity.nodeId);
  if (it != m_peerViewReverse.end()) {
    m_peerViewReverse.erase(it);
  } else {
    BOOST_LOG_SEV(m_logger, log::debug) << "Triggering vicinity-directed full NDB update to " << vicinity.nodeId.hex() << ".";
    m_router.context().schedule(
      m_router.context().roughly(15),
      boost::bind(&NameDatabase::fullUpdate, &m_nameDb, vicinity.nodeId)
    );
  }
}

void SloppyGroupManagerPrivate::refreshForeignPeerView()
{
  // TODO

  // TODO: After the foreign peer view has been refreshed, do a full update
}

void SloppyGroupManagerPrivate::announceFullRecords()
{
  BOOST_LOG_SEV(m_logger, log::debug) << "Triggering global full NDB update.";
  m_nameDb.fullUpdate();
}

void SloppyGroupManagerPrivate::nibExportQueueRecord(const SloppyPeer &peer,
                                                     const Protocol::NameAnnounce &announce)
{
  RecursiveUniqueLock lock(m_mutex);

  // Perform entry aggregation into a single announce message containing multiple
  // name announces
  NameAggregationBufferPtr buffer;
  auto it = m_nibExportAggregate.find(peer.nodeId);
  if (it == m_nibExportAggregate.end()) {
    buffer = boost::make_shared<NameAggregationBuffer>(m_router.context(), peer);
    m_nibExportAggregate.insert({{ peer.nodeId, buffer }});
  } else {
    buffer = it->second;
  }

  // Replace existing announces with new ones, so only the lastest are transmitted
  buffer->announces[announce.origin_id()] = announce;

  // Buffer further messages for another 15 seconds, then transmit all of them
  if (!buffer->buffering) {
    buffer->buffering = true;
    buffer->timer.expires_from_now(m_router.context().roughly(15));
    buffer->timer.async_wait(boost::bind(&SloppyGroupManagerPrivate::nibExportTransmitBuffer, this, _1, buffer));
  }
}

void SloppyGroupManagerPrivate::nibExportTransmitBuffer(const boost::system::error_code &error,
                                                        NameAggregationBufferPtr buffer)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  Protocol::AggregateNameAnnounce aggregate;
  for (const auto &pa : buffer->announces) {
    *aggregate.add_announces() = pa.second;
  }

  m_router.route(
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    buffer->peer.nodeId,
    buffer->peer.landmarkAddress(),
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    static_cast<std::uint32_t>(SloppyGroupManagerPrivate::MessageType::NameAnnounce),
    aggregate,
    RoutingOptions().setTrackHopDistance(true)
  );

  m_statistics.recordXmits += buffer->announces.size();

  // Clear the buffer after transmission
  buffer->buffering = false;
  buffer->announces.clear();
}

PeerView SloppyGroupManagerPrivate::getLocalPeerView() const
{
  PeerView peerView;
  for (const CompactRoutingTable::VicinityDescriptor &vicinity : m_router.routingTable().getVicinity()) {
    if (vicinity.nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
      continue;

    peerView.insert(SloppyPeer(vicinity.nodeId, LandmarkAddress(), vicinity.hops));
  }

  return peerView;
}

void SloppyGroupManagerPrivate::nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Prepare announcement message
  Protocol::NameAnnounce announce;
  announce.set_origin_id(record->nodeId.raw());
  announce.set_timestamp(record->timestamp);
  announce.set_seqno(record->seqno);
  for (const LandmarkAddress &address : record->addresses) {
    Protocol::LandmarkAddress *laddr = announce.add_addresses();
    laddr->set_landmarkid(address.landmarkId().raw());
    for (Vport port : address.path())
      laddr->add_address(port);
  }

  if (peerId.isNull()) {
    // Export record to the whole peer view
    // TODO: Should we unify this?
    for (const SloppyPeer &peer : getLocalPeerView()) {
      if (record->receivedPeerId != peer.nodeId && record->nodeId != peer.nodeId)
        nibExportQueueRecord(peer, announce);
    }
    for (const SloppyPeer &peer : m_peerViewForeign) {
      if (record->receivedPeerId != peer.nodeId && record->nodeId != peer.nodeId)
        nibExportQueueRecord(peer, announce);
    }
    for (const SloppyPeer &peer : m_peerViewReverse) {
      if (record->receivedPeerId != peer.nodeId && record->nodeId != peer.nodeId)
        nibExportQueueRecord(peer, announce);
    }
  } else if (record->nodeId == peerId) {
    return;
  } else {
    SloppyPeer peer(peerId, LandmarkAddress(), 0);
    auto it = m_peerViewForeign.find(peerId);
    if (it == m_peerViewForeign.end()) {
      it = m_peerViewReverse.find(peerId);
      if (it != m_peerViewReverse.end())
        peer = *it;
    } else {
      peer = *it;
    }

    nibExportQueueRecord(peer, announce);
  }
}

void SloppyGroupManagerPrivate::messageDelivery(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup))
    return;

  switch (msg.payloadType()) {
    case static_cast<std::uint32_t>(MessageType::NameAnnounce): {
      // Accept message only if source node belongs to this sloppy group
      if (msg.sourceNodeId().prefix(m_groupPrefixLength) != m_groupPrefix)
        return;
      // Accept message only if it tracks the hop distance
      if (!msg.hopDistance())
        return;

      Protocol::AggregateNameAnnounce announces = message_cast<Protocol::AggregateNameAnnounce>(msg);
      m_statistics.recordRcvd += announces.announces_size();
      std::unordered_map<NodeIdentifier, NameRecordPtr> sourceSummary;

      for (int i = 0; i < announces.announces_size(); i++) {
        const Protocol::NameAnnounce &announce = announces.announces(i);

        // Accept announces only for peers in the same sloppy group
        NodeIdentifier originId(announce.origin_id());
        if (originId.prefix(m_groupPrefixLength) != m_groupPrefix) {
          BOOST_LOG_SEV(m_logger, log::warning) << "Dropping name record (origin not in our SG).";
          continue;
        }

        NameRecordPtr record = boost::make_shared<NameRecord>(m_router.context(),
          originId, NameRecord::Type::SloppyGroup);
        record->receivedPeerId = msg.sourceNodeId();
        record->timestamp = announce.timestamp();
        record->seqno = announce.seqno();
        sourceSummary.insert({{ originId, record }});

        for (int j = 0; j < announce.addresses_size(); j++) {
          const Protocol::LandmarkAddress &laddr = announce.addresses(j);
          record->addresses.push_back(LandmarkAddress(
            NodeIdentifier(laddr.landmarkid()),
            laddr.address()
          ));
        }

        // Store record into the name database
        m_nameDb.store(record);
      }

      // Establish backlinks when necessary
      bool accept = false;
      {
        RecursiveUniqueLock lock(m_mutex);
        PeerView localPeerView = getLocalPeerView();
        if (localPeerView.find(msg.sourceNodeId()) == localPeerView.end()) {
          if (m_peerViewReverse.find(msg.sourceNodeId()) == m_peerViewReverse.end()) {
            // TODO: Expire obsolete reverse links (reverse links are ignored in update times)

            // Only accept a limited number of peers
            if (m_peerViewReverse.size() >= m_maxPeerViewSize) {
              auto &peerHops = m_peerViewReverse.get<PVTags::HopCount>();
              auto it = --peerHops.end();
              if (msg.hopDistance() < it->hops) {
                peerHops.erase(it);
                accept = true;
              }
            } else {
              accept = true;
            }

            if (accept) {
              m_peerViewReverse.insert(SloppyPeer(msg.sourceNodeId(), msg.sourceAddress(), msg.hopDistance()));
            }
          }
        }
      }

      // Check if we need to send an update to the just added peer
      if (accept) {
        // Compute record diff to see if we actually need to send any records to the destination
        auto recordIds = m_nameDb.diff(sourceSummary);

        if (recordIds.size() > 0) {
          BOOST_LOG_SEV(m_logger, log::debug) << "Triggering reverse-directed diff (" << recordIds.size() << "/" << m_nameDb.sizeActive() << ") NDB update to " << msg.sourceNodeId().hex() << ".";
          // This call must be deferred so that we don't acquire the NDB lock while we are also
          // holding the SG lock -- this could lead to a deadlock
          m_router.context().schedule(
            m_router.context().roughly(15),
            boost::bind(&NameDatabase::diffUpdate, &m_nameDb, recordIds, msg.sourceNodeId())
          );
        }
      }
      break;
    }
  }
}

void SloppyGroupManagerPrivate::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  RecursiveUniqueLock lock(m_mutex);

  stream << "*** Sloppy group:" << std::endl;
  stream << "Prefix length: " << m_groupPrefixLength << std::endl;
  stream << "Prefix: " << m_groupPrefix.hex() << std::endl;
  stream << "Max. peer view size: " << m_maxPeerViewSize << std::endl;
  stream << "Local peer view size: " << getLocalPeerView().size() << std::endl;
  stream << "Foreign peer view size: " << m_peerViewForeign.size() << std::endl;
  stream << "Reverse peer view size: " << m_peerViewReverse.size() << std::endl;

  stream << "*** Peer views:" << std::endl;
  for (const SloppyPeer &peer : getLocalPeerView()) {
    stream << "  L " << peer.nodeId.hex() << " hops=" << peer.hops;
    stream << std::endl;
  }
  for (const SloppyPeer &peer : m_peerViewForeign.get<PVTags::HopCount>()) {
    stream << "  F " << peer.nodeId.hex() << " hops=" << peer.hops << " laddrs=" << peer.addresses;
    stream << std::endl;
  }
  for (const SloppyPeer &peer : m_peerViewReverse.get<PVTags::HopCount>()) {
    stream << "  R " << peer.nodeId.hex() << " hops=" << peer.hops << " laddrs=" << peer.addresses;
    stream << std::endl;
  }
}

void SloppyGroupManagerPrivate::dumpTopology(SloppyGroupManager::TopologyDumpGraph &graph,
                                             std::function<std::string(const NodeIdentifier&)> resolve)
{
  RecursiveUniqueLock lock(m_mutex);

  if (!resolve)
    resolve = [&](const NodeIdentifier &nodeId) { return nodeId.hex(); };

  using Tags = SloppyGroupManager::TopologyDumpTags;
  std::string name = resolve(m_localId);
  auto self = graph.add_vertex(name);
  boost::put(boost::get(Tags::NodeName(), graph.graph()), self, name);
  boost::put(boost::get(Tags::NodeGroup(), graph.graph()), self,
    m_groupPrefix.bin().substr(0, m_groupPrefixLength));
  boost::put(boost::get(Tags::NodeGroupPrefixLength(), graph.graph()), self, m_groupPrefixLength);
  boost::put(boost::get(Tags::Placeholder(), graph.graph()), self, false);

  auto addEdge = [&](const NodeIdentifier &id, bool foreign = false, bool reverse = false) {
    auto vertex = graph.add_vertex(resolve(id));
    boost::put(boost::get(Tags::NodeName(), graph.graph()), vertex, resolve(id));

    boost::optional<bool> placeholder = boost::get(Tags::Placeholder(), graph.graph(), vertex);
    if (!placeholder)
      boost::put(boost::get(Tags::Placeholder(), graph.graph()), vertex, true);

    auto edge = boost::add_edge(self, vertex, graph).first;
    boost::put(boost::get(Tags::LinkIsForeign(), graph.graph()), edge, foreign);
    boost::put(boost::get(Tags::LinkIsReverse(), graph.graph()), edge, reverse);
  };

  for (const auto &peer : getLocalPeerView()) {
    addEdge(peer.nodeId);
  }
  for (const auto &peer : m_peerViewForeign) {
    addEdge(peer.nodeId, true);
  }
  for (const auto &peer : m_peerViewReverse) {
    addEdge(peer.nodeId, false, true);
  }
}

SloppyGroupManager::SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : d(new SloppyGroupManagerPrivate(router, sizeEstimator))
{
}

void SloppyGroupManager::initialize()
{
  d->initialize();
}

void SloppyGroupManager::shutdown()
{
  d->shutdown();
}

size_t SloppyGroupManager::getGroupPrefixLength() const
{
  return d->m_groupPrefixLength;
}

const NodeIdentifier &SloppyGroupManager::getGroupPrefix() const
{
  return d->m_groupPrefix;
}

size_t SloppyGroupManager::sizePeerViews() const
{
  return d->getLocalPeerView().size() +
         d->m_peerViewForeign.size() +
         d->m_peerViewReverse.size();
}

const SloppyGroupManager::Statistics &SloppyGroupManager::statistics() const
{
  return d->m_statistics;
}

void SloppyGroupManager::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  d->dump(stream, resolve);
}

void SloppyGroupManager::dumpTopology(TopologyDumpGraph &graph, std::function<std::string(const NodeIdentifier&)> resolve)
{
  d->dumpTopology(graph, resolve);
}

}
