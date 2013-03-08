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
#include "social/sloppy_group.h"
#include "social/compact_router.h"
#include "social/address.h"
#include "social/name_database.h"
#include "interplex/link_manager.h"
#include "core/operators.h"

#include "src/social/messages.pb.h"

#include <set>
#include <boost/asio.hpp>

namespace UniSphere {

class SloppyPeer {
public:
  SloppyPeer();

  explicit SloppyPeer(const NodeIdentifier &nodeId);

  SloppyPeer(const NodeIdentifier &nodeId, const LandmarkAddress &address);

  /**
   * Constructs a sloppy peer from a name record.
   *
   * @param record Name record pointer
   */
  explicit SloppyPeer(NameRecordPtr record);

  bool isNull() const { return nodeId.isNull(); }

  void clear();

  LandmarkAddress landmarkAddress() const;

  bool operator<(const SloppyPeer &other) const
  {
    return nodeId < other.nodeId;
  }

  bool operator>(const SloppyPeer &other) const
  {
    return nodeId > other.nodeId;
  }

  bool operator==(const SloppyPeer &other) const
  {
    return nodeId == other.nodeId;
  }
public:
  /// Sloppy peer node identifier
  NodeIdentifier nodeId;
  /// A list of L-R addresses for this sloppy peer
  std::list<LandmarkAddress> addresses;
};

class BlacklistedPeer {
public:
  explicit BlacklistedPeer(const NodeIdentifier &peerId)
    : peerId(peerId)
  {
  }

  BlacklistedPeer(Context &context, const NodeIdentifier &peerId)
    : peerId(peerId),
      timer(new boost::asio::deadline_timer(context.service()))
  {
  }

  bool operator<(const BlacklistedPeer &other) const
  {
    return peerId < other.peerId;
  }

  bool operator==(const BlacklistedPeer &other) const
  {
    return peerId == other.peerId;
  }
public:
  NodeIdentifier peerId;
  boost::shared_ptr<boost::asio::deadline_timer> timer;
};

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
    /// Finger reject
    FingerReject  = 0x02
  };

  SloppyGroupManagerPrivate(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  void initialize();

  void shutdown();

  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve);

  void dumpTopology(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve);

  void refreshNeighborSet(const boost::system::error_code &error);

  void refreshOneLongFinger(std::function<void(const std::list<NameRecordPtr>&, const NodeIdentifier&)> handler,
    RpcCallGroupPtr rpcGroup = RpcCallGroupPtr());

  NameRecordPtr filterLongFingers(const std::list<NameRecordPtr> &records,
    const NodeIdentifier &referenceId);

  void ndbHandleResponseShort(const std::list<NameRecordPtr> &records);

  void ndbHandleResponseLong(const std::list<NameRecordPtr> &records, const NodeIdentifier &targetId);

  void ndbRefreshCompleted();

  /**
   * Announces local sloppy group name records to the neighbor set.
   */
  void announceFullRecords(const boost::system::error_code &error);

  void nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId);

  void networkSizeEstimateChanged(std::uint64_t size);

  void rejectPeerLink(const RoutedMessage &msg);

  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const RoutedMessage &msg);
public:
  /// Router instance
  CompactRouter &m_router;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Name database reference
  NameDatabase &m_nameDb;
  /// Mutex protecting the sloppy group manager
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;
  /// Predecessor in the overlay
  SloppyPeer m_predecessor;
  /// Successor in the overlay
  SloppyPeer m_successor;
  /// Outgoing fingers (including predecessor and successor)
  std::map<NodeIdentifier, SloppyPeer> m_fingersOut;
  /// Incoming fingers
  std::map<NodeIdentifier, SloppyPeer> m_fingersIn;
  /// The set of newly discovered short fingers
  std::set<SloppyPeer> m_newShortFingers;
  /// The set of newly discovered long fingers
  std::map<NodeIdentifier, std::list<NameRecordPtr>> m_newLongFingers;
  /// A list of peers that should be temporarily excluded from becoming fingers
  std::set<BlacklistedPeer> m_blacklistedPeers;
  /// Timer for periodic neighbor set refresh
  boost::asio::deadline_timer m_neighborRefreshTimer;
  /// Timer for periodic annouces
  boost::asio::deadline_timer m_announceTimer;
  /// Active subscriptions to other components
  std::list<boost::signals::connection> m_subscriptions;
  /// Sloppy group prefix length
  size_t m_groupPrefixLength;
  /// Sloppy group prefix
  NodeIdentifier m_groupPrefix;
  /// Expected sloppy group size
  double m_groupSize;
};

SloppyPeer::SloppyPeer()
{
}

SloppyPeer::SloppyPeer(const NodeIdentifier &nodeId)
  : nodeId(nodeId)
{
}

SloppyPeer::SloppyPeer(const NodeIdentifier &nodeId, const LandmarkAddress &address)
  : nodeId(nodeId),
    addresses({ address })
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
    m_sizeEstimator(sizeEstimator),
    m_nameDb(router.nameDb()),
    m_localId(router.identity().localId()),
    m_neighborRefreshTimer(router.context().service()),
    m_announceTimer(router.context().service()),
    m_groupPrefixLength(0)
{
}

void SloppyGroupManagerPrivate::initialize()
{
  RecursiveUniqueLock lock(m_mutex);

  UNISPHERE_LOG(m_router.linkManager(), Info, "SloppyGroupManager: Initializing sloppy group manager.");

  // Subscribe to all events
  m_subscriptions
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&SloppyGroupManagerPrivate::networkSizeEstimateChanged, this, _1))
    << m_nameDb.signalExportRecord.connect(boost::bind(&SloppyGroupManagerPrivate::nibExportRecord, this, _1, _2))
    << m_router.signalDeliverMessage.connect(boost::bind(&SloppyGroupManagerPrivate::messageDelivery, this, _1))
  ;

  // Initialize sloppy group prefix length
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start periodic neighbor set refresh timer
  m_neighborRefreshTimer.expires_from_now(boost::posix_time::seconds(30));
  m_neighborRefreshTimer.async_wait(boost::bind(&SloppyGroupManagerPrivate::refreshNeighborSet, this, _1));
}

void SloppyGroupManagerPrivate::shutdown()
{
  RecursiveUniqueLock lock(m_mutex);

  UNISPHERE_LOG(m_router.linkManager(), Warning, "SloppyGroupManager: Shutting down sloppy group manager.");

  // Unsubscribe from all events
  for (boost::signals::connection c : m_subscriptions)
    c.disconnect();
  m_subscriptions.clear();

  // Cancel refresh timer
  m_neighborRefreshTimer.cancel();

  // Clear the neighbor set
  m_predecessor.clear();
  m_successor.clear();
  m_fingersOut.clear();
  m_fingersIn.clear();
  m_blacklistedPeers.clear();
}

void SloppyGroupManagerPrivate::networkSizeEstimateChanged(std::uint64_t size)
{
  RecursiveUniqueLock lock(m_mutex);
  double n = static_cast<double>(size);
  m_groupPrefixLength = static_cast<int>(std::floor(std::log(std::sqrt(n / std::log(n))) / std::log(2.0)));
  // TODO: Only change the prefix length when n changes by at least some constant factor (eg. 10%)

  m_groupPrefix = m_localId.prefix(m_groupPrefixLength);
  m_groupSize = std::sqrt(n * std::log(n));

  // TODO: Refresh neighbor set when the group prefix length has been changed
}

void SloppyGroupManagerPrivate::refreshNeighborSet(const boost::system::error_code &error)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  auto group = rpc.group(boost::bind(&SloppyGroupManagerPrivate::ndbRefreshCompleted, this));

  // Lookup successor and predecessor
  m_nameDb.remoteLookupSloppyGroup(m_localId, m_groupPrefixLength,
    NameDatabase::LookupType::ClosestNeighbors,
    group, boost::bind(&SloppyGroupManagerPrivate::ndbHandleResponseShort, this, _1));

  for (int i = 0; i < SloppyGroupManager::finger_count; i++) {
    refreshOneLongFinger(boost::bind(&SloppyGroupManagerPrivate::ndbHandleResponseLong, this, _1, _2), group);
  }

  // Reschedule neighbor set refresh
  m_neighborRefreshTimer.expires_from_now(boost::posix_time::seconds(600));
  m_neighborRefreshTimer.async_wait(boost::bind(&SloppyGroupManagerPrivate::refreshNeighborSet, this, _1));
}

void SloppyGroupManagerPrivate::refreshOneLongFinger(std::function<void(const std::list<NameRecordPtr>&, const NodeIdentifier&)> handler,
                                                     RpcCallGroupPtr rpcGroup)
{
  // Compute long distance finger identifier based on a harmonic probability distribution
  NodeIdentifier fingerId = m_groupPrefix;
  double r = std::generate_canonical<double, 32>(m_router.context().basicRng());
  fingerId += std::exp(std::log(m_groupSize) * (r - 1.0)) * std::pow(2, NodeIdentifier::bit_length - m_groupPrefixLength);

  m_nameDb.remoteLookupSloppyGroup(fingerId, m_groupPrefixLength, NameDatabase::LookupType::Closest, rpcGroup,
    boost::bind(handler, _1, fingerId));
}

void SloppyGroupManagerPrivate::ndbHandleResponseShort(const std::list<NameRecordPtr> &records)
{
  RecursiveUniqueLock lock(m_mutex);

  for (NameRecordPtr record : records) {
    // Skip records that are not in our sloppy group
    if (record->nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
      continue;

    // Skip records that are equal to our node identifier
    if (record->nodeId == m_localId)
      continue;

    m_newShortFingers.insert(SloppyPeer(record));
  }
}

void SloppyGroupManagerPrivate::ndbHandleResponseLong(const std::list<NameRecordPtr> &records, const NodeIdentifier &targetId)
{
  RecursiveUniqueLock lock(m_mutex);
  m_newLongFingers[targetId] = records;
}

void SloppyGroupManagerPrivate::ndbRefreshCompleted()
{
  RecursiveUniqueLock lock(m_mutex);

  // Ensure that we have enough short fingers
  if (m_newShortFingers.size() < 2)
    return;

  // Determine successor and predecessor from our fingers list
  auto it = m_newShortFingers.upper_bound(SloppyPeer(m_localId));
  if (it == m_newShortFingers.end())
    --it;

  // Check if previous entry is closer
  auto pit = it;
  if (it != m_newShortFingers.begin()) {
    if ((*(--pit)).nodeId.distanceTo(m_localId) < (*it).nodeId.distanceTo(m_localId))
      it = pit;
  }

  // Iterator now contains the closest element to self; determine
  // successor and predecessor

  // Predecessor
  if ((*it).nodeId < m_localId) {
    m_predecessor = *it;
  } else {
    pit = it;
    if (pit == m_newShortFingers.begin())
      pit = m_newShortFingers.end();

    m_predecessor = *(--pit);
  }

  // Successor
  if ((*it).nodeId > m_localId) {
    m_successor = *it;
  } else {
    pit = it;
    if (++pit != m_newShortFingers.end())
      m_successor = *pit;
    else
      m_successor = *m_newShortFingers.begin();
  }

  m_fingersOut.clear();
  m_fingersOut.insert({{ m_predecessor.nodeId, m_predecessor }});
  m_fingersOut.insert({{ m_successor.nodeId, m_successor }});

  // Determine long fingers
  for (auto pr : m_newLongFingers) {
    NameRecordPtr closest = filterLongFingers(pr.second, pr.first);

    if (closest)
      m_fingersOut.insert({{ closest->nodeId, SloppyPeer(closest) }});
  }

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  // TODO: If there are no long fingers, retry the selection (a limited amount of times)

  // Preempt full records announce
  // TODO: Do this only when the neighbor set has changed
  m_announceTimer.expires_from_now(boost::posix_time::seconds(15));
  m_announceTimer.async_wait(boost::bind(&SloppyGroupManagerPrivate::announceFullRecords, this, _1));
}

NameRecordPtr SloppyGroupManagerPrivate::filterLongFingers(const std::list<NameRecordPtr> &records, const NodeIdentifier &referenceId)
{
  RecursiveUniqueLock lock(m_mutex);
  NameRecordPtr closest;
  for (NameRecordPtr record : records) {
    // Skip records that are not in our sloppy group
    if (record->nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
      continue;

    // Skip records that are equal to our node identifier
    if (record->nodeId == m_localId)
      continue;

    // Skip records that are already among the outgoing fingers
    if (m_fingersOut.find(record->nodeId) != m_fingersOut.end())
      continue;

    // Skip records that are blacklisted
    if (m_blacklistedPeers.find(BlacklistedPeer(record->nodeId)) != m_blacklistedPeers.end())
      continue;

    // Skip records that are further away
    if (closest && closest->nodeId.distanceTo(referenceId) < record->nodeId.distanceTo(referenceId))
      continue;

    closest = record;
  }

  return closest;
}

void SloppyGroupManagerPrivate::announceFullRecords(const boost::system::error_code &error)
{
  if (error)
    return;
  
  // Announce full updates to the neighbor set
  for (const auto &pf : m_fingersOut) {
    m_nameDb.fullUpdate(pf.second.nodeId);
  }

  for (const auto &pf : m_fingersIn) {
    m_nameDb.fullUpdate(pf.second.nodeId);
  }

  // Schedule next periodic export
  m_announceTimer.expires_from_now(boost::posix_time::seconds(SloppyGroupManager::interval_announce));
  m_announceTimer.async_wait(boost::bind(&SloppyGroupManagerPrivate::announceFullRecords, this, _1));
}

void SloppyGroupManagerPrivate::nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId)
{
  auto exportRecord = [&](const SloppyPeer &peer) {
    if (!record->originId.isNull()) {
      int forwardDirection = (record->originId > m_localId) ? -1 : 1;

      // Only forward in the given direction, never backtrack
      if (forwardDirection == 1 && peer.nodeId < m_localId)
        return;
      else if (forwardDirection == -1 && peer.nodeId > m_localId)
        return;
    }

    // Export record to selected peer
    Protocol::NameAnnounce announce;
    announce.set_originid(record->nodeId.raw());
    for (const LandmarkAddress &address : record->addresses) {
      Protocol::LandmarkAddress *laddr = announce.add_addresses();
      laddr->set_landmarkid(address.landmarkId().raw());
      for (Vport port : address.path())
        laddr->add_address(port);
    }

    m_router.route(
      static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
      peer.nodeId,
      peer.landmarkAddress(),
      static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
      static_cast<std::uint32_t>(SloppyGroupManagerPrivate::MessageType::NameAnnounce),
      announce
    );
  };

  if (peerId.isNull()) {
    // Export record to all overlay fingers
    for (const auto &pf : m_fingersOut) {
      exportRecord(pf.second);
    }
    for (const auto &pf : m_fingersIn) {
      exportRecord(pf.second);
    }
  } else {
    auto it = m_fingersOut.find(peerId);
    if (it != m_fingersOut.end()) {
      exportRecord(it->second);
    } else {
      auto jt = m_fingersIn.find(peerId);
      if (jt != m_fingersIn.end())
        exportRecord(jt->second);
    }
  }
}

void SloppyGroupManagerPrivate::rejectPeerLink(const RoutedMessage &msg)
{
  Protocol::SloppyGroupRejectFinger rejection;
  m_router.route(
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    msg.sourceNodeId(),
    msg.sourceAddress(),
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    static_cast<std::uint32_t>(SloppyGroupManagerPrivate::MessageType::FingerReject),
    rejection
  );
}

void SloppyGroupManagerPrivate::messageDelivery(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup))
    return;

  switch (msg.payloadType()) {
    case static_cast<std::uint32_t>(MessageType::NameAnnounce): {
      // Accept message only if source node belongs to this sloppy group
      if (msg.sourceNodeId().prefix(m_groupPrefixLength) != m_groupPrefix)
        return rejectPeerLink(msg);

      // Check if this peer is already registered among the incoming or outgoing fingers
      {
        RecursiveUniqueLock lock(m_mutex);
        auto jt = m_fingersOut.find(msg.sourceNodeId());
        if (jt != m_fingersOut.end()) {
          // Update source address of the finger as it should be more recent
          if (jt->second.landmarkAddress() != msg.sourceAddress()) {
            jt->second.addresses.clear();
            jt->second.addresses.push_back(msg.sourceAddress());
          }
        } else {
          auto it = m_fingersIn.find(msg.sourceNodeId());
          if (it == m_fingersIn.end()) {
            // Reject the peer if there are too many incoming fingers already
            if (m_fingersIn.size() >= SloppyGroupManager::finger_count)
              return rejectPeerLink(msg);

            // Create a new incoming peer entry
            m_fingersIn.insert({{ msg.sourceNodeId(), SloppyPeer(msg.sourceNodeId(), msg.sourceAddress()) }});
          } else {
            // Update source address of the finger as it should be more recent
            if (it->second.landmarkAddress() != msg.sourceAddress()) {
              it->second.addresses.clear();
              it->second.addresses.push_back(msg.sourceAddress());
            }
          }
        }
      }

      Protocol::NameAnnounce announce = message_cast<Protocol::NameAnnounce>(msg);

      std::list<LandmarkAddress> addresses;
      for (int j = 0; j < announce.addresses_size(); j++) {
        const Protocol::LandmarkAddress &laddr = announce.addresses(j);
        addresses.push_back(LandmarkAddress(
          NodeIdentifier(laddr.landmarkid(), NodeIdentifier::Format::Raw),
          laddr.address()
        ));
      }

      // Store record into the name database
      m_nameDb.store(
        NodeIdentifier(announce.originid(), NodeIdentifier::Format::Raw),
        addresses,
        NameRecord::Type::SloppyGroup,
        msg.sourceNodeId()
      );
      break;
    }

    case static_cast<std::uint32_t>(MessageType::FingerReject): {
      // One of our outgoing fingers got rejected by the destination node; remove the finger
      // from our outgoing list and retry the selection of a single finger
      RecursiveUniqueLock lock(m_mutex);

      auto fit = m_fingersOut.find(msg.sourceNodeId());
      if (fit == m_fingersOut.end())
        return;

      m_fingersOut.erase(fit);
      // Move the removed peer to a blacklist, otherwise this same finger may be
      // selected again and again which will cause a nasty loop
      NodeIdentifier peerId = msg.sourceNodeId();
      BlacklistedPeer blacklisted(m_router.context(), peerId);
      blacklisted.timer->expires_from_now(boost::posix_time::seconds(SloppyGroupManager::interval_announce));
      blacklisted.timer->async_wait([this, peerId](const boost::system::error_code&) {
        RecursiveUniqueLock lock(m_mutex);
        m_blacklistedPeers.erase(BlacklistedPeer(peerId));
      });
      m_blacklistedPeers.insert(blacklisted);

      // TODO: Rate limit this so we don't flood the landmarks
      refreshOneLongFinger([this](const std::list<NameRecordPtr> &records, const NodeIdentifier &referenceId) {
        RecursiveUniqueLock lock(m_mutex);
        NameRecordPtr record = filterLongFingers(records, referenceId);
        if (record) {
          m_fingersOut.insert({{ record->nodeId, SloppyPeer(record) }});
          m_nameDb.fullUpdate(record->nodeId);
        }
      });
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
  stream << "Predecessor: " << m_predecessor.nodeId.hex();
  if (resolve && !m_predecessor.isNull())
    stream << " (" << resolve(m_predecessor.nodeId) << ")";
  stream << std::endl;
  stream << "Successor: " << m_successor.nodeId.hex();
  if (resolve && !m_successor.isNull())
    stream << " (" << resolve(m_successor.nodeId) << ")";
  stream << std::endl;

  stream << "*** Sloppy group fingers:" << std::endl;
  for (const auto &pf : m_fingersOut) {
    if (pf.second == m_successor || pf.second == m_predecessor)
      continue;

    stream << "  " << pf.second.nodeId.hex();
    if (resolve)
      stream << " (" << resolve(pf.second.nodeId) << ")";

    stream << " laddr=" << pf.second.landmarkAddress();
    stream << std::endl;
  }
}

void SloppyGroupManagerPrivate::dumpTopology(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  RecursiveUniqueLock lock(m_mutex);

  if (!resolve)
    resolve = [&](const NodeIdentifier &nodeId) { return nodeId.hex(); };

  std::string localId = resolve(m_localId);
  stream << localId << ";" << std::endl;
  if (!m_predecessor.isNull())
    stream << localId << " -> " << resolve(m_predecessor.nodeId) << ";" << std::endl;
  if (!m_successor.isNull())
    stream << localId << " -> " << resolve(m_successor.nodeId) << ";" << std::endl;
  for (const auto &pf : m_fingersOut) {
    if (pf.second == m_successor || pf.second == m_predecessor)
      continue;

    stream << localId << " -> " << resolve(pf.second.nodeId) << " [style=dashed,color=red];" << std::endl;
  }
}

SloppyGroupManager::SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : d(*new SloppyGroupManagerPrivate(router, sizeEstimator))
{
}

void SloppyGroupManager::initialize()
{
  d.initialize();
}

void SloppyGroupManager::shutdown()
{
  d.shutdown();
}

void SloppyGroupManager::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  d.dump(stream, resolve);
}

void SloppyGroupManager::dumpTopology(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  d.dumpTopology(stream, resolve);
}

}
