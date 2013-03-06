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

namespace UniSphere {

SloppyPeer::SloppyPeer()
{
}

SloppyPeer::SloppyPeer(const NodeIdentifier &nodeId)
  : nodeId(nodeId)
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

bool SloppyPeer::operator<(const SloppyPeer &other) const
{
  return nodeId < other.nodeId;
}

bool SloppyPeer::operator>(const SloppyPeer &other) const
{
  return nodeId > other.nodeId;
}

SloppyGroupManager::SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : m_router(router),
    m_sizeEstimator(sizeEstimator),
    m_localId(router.identity().localId()),
    m_neighborRefreshTimer(router.context().service()),
    m_groupPrefixLength(0)
{
}

void SloppyGroupManager::initialize()
{
  UNISPHERE_LOG(m_router.linkManager(), Info, "SloppyGroupManager: Initializing sloppy group manager.");

  // Subscribe to all events
  m_subscriptions
    << m_sizeEstimator.signalSizeChanged.connect(boost::bind(&SloppyGroupManager::networkSizeEstimateChanged, this, _1))
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
  NameDatabase &ndb = m_router.nameDb();

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  auto group = rpc.group(boost::bind(&SloppyGroupManager::ndbRefreshCompleted, this));

  // Lookup successor and predecessor
  ndb.remoteLookupSloppyGroup(m_localId, m_groupPrefixLength,
    NameDatabase::LookupType::ClosestNeighbors,
    group, boost::bind(&SloppyGroupManager::ndbHandleResponseShort, this, _1));

  for (int i = 0; i < SloppyGroupManager::finger_count; i++) {
    // Compute long distance finger identifier based on a harmonic probability distribution
    NodeIdentifier fingerId = m_groupPrefix;
    double r = std::generate_canonical<double, 32>(m_router.context().basicRng());
    fingerId += std::exp(std::log(m_groupSize) * (r - 1.0)) * std::pow(2, NodeIdentifier::bit_length - m_groupPrefixLength);

    ndb.remoteLookupSloppyGroup(fingerId, m_groupPrefixLength, NameDatabase::LookupType::Closest,
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
      m_fingersOut.insert(SloppyPeer(closest));
  }

  m_newShortFingers.clear();
  m_newLongFingers.clear();

  // TODO: If there are no long fingers, retry the selection (a limited amount of times)

  announceToNeighborSet(boost::system::error_code());
}

void SloppyGroupManager::announceToNeighborSet(const boost::system::error_code &error)
{
  RecursiveUniqueLock lock(m_mutex);
  
  // TODO: Announce full updates to the neighbor set
  NameDatabase &ndb = m_router.nameDb();
  auto records = ndb.getNIB().get<NIBTags::TypeDestination>().equal_range(NameRecord::Type::SloppyGroup);
  for (auto it = records.first; it != records.second; ++it) {
    NameRecordPtr record = *it;
    int forwardDirection = (record->originId > m_localId) ? -1 : 1;

    for (const SloppyPeer &peer : m_fingersOut) {
      // Only forward in the given direction, never backtrack
      if (forwardDirection == 1 && peer.nodeId < m_localId)
        continue;
      else if (forwardDirection == -1 && peer.nodeId > m_localId)
        continue;

      nibExportRecord(record, peer);
    }
  }

  // TODO: Schedule next periodic export
}

void SloppyGroupManager::nibExportRecord(NameRecordPtr record, const SloppyPeer &peer)
{
  // TODO
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
  for (const SloppyPeer &peer : m_fingersOut) {
    stream << "  " << peer.nodeId.hex();
    if (resolve)
      stream << " (" << resolve(peer.nodeId) << ")";

    stream << " laddr=" << peer.landmarkAddress();
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
  for (const SloppyPeer &peer : m_fingersOut) {
    stream << localId << " -> " << resolve(peer.nodeId) << " [style=dashed,color=red];" << std::endl;
  }
}

}
