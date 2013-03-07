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
#include "interplex/link_manager.h"
#include "core/operators.h"

#include "src/social/messages.pb.h"

namespace UniSphere {

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

SloppyGroupManager::SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : m_router(router),
    m_sizeEstimator(sizeEstimator),
    m_nameDb(router.nameDb()),
    m_localId(router.identity().localId()),
    m_neighborRefreshTimer(router.context().service()),
    m_announceTimer(router.context().service()),
    m_groupPrefixLength(0)
{
}

void SloppyGroupManager::initialize()
{
  UNISPHERE_LOG(m_router.linkManager(), Info, "SloppyGroupManager: Initializing sloppy group manager.");

  // Subscribe to all events
  m_subscriptions
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&SloppyGroupManager::networkSizeEstimateChanged, this, _1))
    << m_nameDb.signalExportRecord.connect(boost::bind(&SloppyGroupManager::nibExportRecord, this, _1, _2))
    << m_router.signalDeliverMessage.connect(boost::bind(&SloppyGroupManager::messageDelivery, this, _1))
  ;

  // Initialize sloppy group prefix length
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Start periodic neighbor set refresh timer
  m_neighborRefreshTimer.expires_from_now(boost::posix_time::seconds(30));
  m_neighborRefreshTimer.async_wait(boost::bind(&SloppyGroupManager::refreshNeighborSet, this, _1));
}

void SloppyGroupManager::shutdown()
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
}

void SloppyGroupManager::networkSizeEstimateChanged(std::uint64_t size)
{
  RecursiveUniqueLock lock(m_mutex);
  double n = static_cast<double>(size);
  m_groupPrefixLength = static_cast<int>(std::floor(std::log(std::sqrt(n / std::log(n))) / std::log(2.0)));
  // TODO: Only change the prefix length when n changes by at least some constant factor (eg. 10%)

  m_groupPrefix = m_localId.prefix(m_groupPrefixLength);
  m_groupSize = std::sqrt(n * std::log(n));

  // TODO: Refresh neighbor set when the group prefix length has been changed
}

void SloppyGroupManager::refreshNeighborSet(const boost::system::error_code &error)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  auto group = rpc.group(boost::bind(&SloppyGroupManager::ndbRefreshCompleted, this));

  // Lookup successor and predecessor
  m_nameDb.remoteLookupSloppyGroup(m_localId, m_groupPrefixLength,
    NameDatabase::LookupType::ClosestNeighbors,
    group, boost::bind(&SloppyGroupManager::ndbHandleResponseShort, this, _1));

  for (int i = 0; i < SloppyGroupManager::finger_count; i++) {
    // Compute long distance finger identifier based on a harmonic probability distribution
    NodeIdentifier fingerId = m_groupPrefix;
    double r = std::generate_canonical<double, 32>(m_router.context().basicRng());
    fingerId += std::exp(std::log(m_groupSize) * (r - 1.0)) * std::pow(2, NodeIdentifier::bit_length - m_groupPrefixLength);

    m_nameDb.remoteLookupSloppyGroup(fingerId, m_groupPrefixLength, NameDatabase::LookupType::Closest,
      group, boost::bind(&SloppyGroupManager::ndbHandleResponseLong, this, _1, fingerId));
  }

  // Reschedule neighbor set refresh
  m_neighborRefreshTimer.expires_from_now(boost::posix_time::seconds(600));
  m_neighborRefreshTimer.async_wait(boost::bind(&SloppyGroupManager::refreshNeighborSet, this, _1));
}

void SloppyGroupManager::ndbHandleResponseShort(const std::list<NameRecordPtr> &records)
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

void SloppyGroupManager::ndbHandleResponseLong(const std::list<NameRecordPtr> &records, const NodeIdentifier &targetId)
{
  RecursiveUniqueLock lock(m_mutex);
  m_newLongFingers[targetId] = records;
}

void SloppyGroupManager::ndbRefreshCompleted()
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

  // Determine long fingers
  m_fingersOut.clear();
  for (auto pr : m_newLongFingers) {
    NameRecordPtr closest;
    for (NameRecordPtr record : pr.second) {
      // Skip records that are not in our sloppy group
      if (record->nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
        continue;

      // Skip records that are equal to our node identifier
      if (record->nodeId == m_localId)
        continue;

      // Skip records that are already among the short fingers
      if (record->nodeId == m_successor.nodeId || record->nodeId == m_predecessor.nodeId)
        continue;

      // Skip records that are further away
      if (closest && closest->nodeId.distanceTo(pr.first) < record->nodeId.distanceTo(pr.first))
        continue;

      closest = record;
    }

    if (closest)
      m_fingersOut.insert({{ closest->nodeId, SloppyPeer(closest) }});
  }

  m_fingersOut.insert({{ m_predecessor.nodeId, m_predecessor }});
  m_fingersOut.insert({{ m_successor.nodeId, m_successor }});

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  // TODO: If there are no long fingers, retry the selection (a limited amount of times)

  // Preempt full records announce
  // TODO: Do this only when the neighbor set has changed
  m_announceTimer.expires_from_now(boost::posix_time::seconds(15));
  m_announceTimer.async_wait(boost::bind(&SloppyGroupManager::announceFullRecords, this, _1));
}

void SloppyGroupManager::announceFullRecords(const boost::system::error_code &error)
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
  m_announceTimer.async_wait(boost::bind(&SloppyGroupManager::announceFullRecords, this, _1));
}

void SloppyGroupManager::nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId)
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
      static_cast<std::uint32_t>(SloppyGroupManager::MessageType::NameAnnounce),
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

void SloppyGroupManager::rejectPeerLink(const RoutedMessage &msg)
{
  Protocol::SloppyGroupRejectFinger rejection;
  m_router.route(
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    msg.sourceNodeId(),
    msg.sourceAddress(),
    static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup),
    static_cast<std::uint32_t>(SloppyGroupManager::MessageType::FingerReject),
    rejection
  );
}

void SloppyGroupManager::messageDelivery(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(CompactRouter::Component::SloppyGroup))
    return;

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

  switch (msg.payloadType()) {
    case static_cast<std::uint32_t>(MessageType::NameAnnounce): {
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
      // One of our outgoing fingers got rejected by the destination node; we should
      // retry the selection of one finger
      // TODO: Buffer this so we don't flood the landmarks
      break;
    }
  }
}

void SloppyGroupManager::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
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

void SloppyGroupManager::dumpTopology(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
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

}
