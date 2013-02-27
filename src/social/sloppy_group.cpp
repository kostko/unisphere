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

  // TODO: Wait for the landmark set to stabilize and then refresh the neighbor set for the first time
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
  m_fingers.clear();
}

void SloppyGroupManager::networkSizeEstimateChanged(std::uint64_t size)
{
  RecursiveUniqueLock lock(m_mutex);
  double n = static_cast<double>(size);
  m_groupPrefixLength = static_cast<int>(std::floor(std::log(std::sqrt(n / std::log(n))) / std::log(2.0)));
  // TODO: Only change the prefix length when n changes by at least some constant factor (eg. 10%)

  m_groupPrefix = m_router.identity().localId().prefix(m_groupPrefixLength);

  // TODO: Refresh neighbor set when the group prefix length has been changed
}

void SloppyGroupManager::refreshNeighborSet(const boost::system::error_code &error)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();
  NameDatabase &ndb = m_router.nameDb();

  auto group = rpc.group(boost::bind(&SloppyGroupManager::ndbRefreshCompleted, this));

  // Lookup successor and predecessor
  ndb.remoteLookupSloppyGroup(m_router.identity().localId(), m_groupPrefixLength,
    NameDatabase::LookupType::ClosestNeighbors,
    group, boost::bind(&SloppyGroupManager::ndbHandleNeighborResponse, this, _1));

  /*for (int i = 0; i < SloppyGroupManager::finger_count; i++) {
    NodeIdentifier fingerId = m_groupPrefix;

    // Compute random distance from current node; smaller distances have greater probabilities of being chosen
    double r = std::generate_canonical<double, 32>(m_router.context().basicRng());
    fingerId += std::exp(std::log(std::pow(2, 160 - m_groupPrefixLength) - 1) * r - 1.0);

    ndb.remoteLookupSloppyGroup(fingerId, m_groupPrefixLength, NameDatabase::LookupType::Closest,
      group, boost::bind(&SloppyGroupManager::ndbHandleFingerResponse, this, _1));
  }*/

  // Reschedule neighbor set refresh
  m_neighborRefreshTimer.expires_from_now(boost::posix_time::seconds(600));
  m_neighborRefreshTimer.async_wait(boost::bind(&SloppyGroupManager::refreshNeighborSet, this, _1));
}

void SloppyGroupManager::ndbHandleNeighborResponse(const std::list<NameRecordPtr> &records)
{
  RecursiveUniqueLock lock(m_mutex);

  if (records.size() < 2)
    return;

  SloppyPeer predecessorCand = SloppyPeer(records.front());
  SloppyPeer successorCand = SloppyPeer(records.back());
  NodeIdentifier self = m_router.identity().localId();

  if (predecessorCand.nodeId < self && (m_predecessor.isNull() || predecessorCand > m_predecessor))
    m_predecessor = predecessorCand;

  if (successorCand.nodeId > self && (m_successor.isNull() || successorCand < m_successor))
    m_successor = successorCand;
}

void SloppyGroupManager::ndbHandleFingerResponse(const std::list<NameRecordPtr> &records)
{
  RecursiveUniqueLock lock(m_mutex);

  for (NameRecordPtr record : records) {
    // Skip records that are not in our sloppy group
    if (record->nodeId.prefix(m_groupPrefixLength) != m_groupPrefix)
      continue;

    // Skip records that are equal to our node identifier
    if (record->nodeId == m_router.identity().localId())
      continue;

    m_newFingers.insert(SloppyPeer(record));
  }
}

void SloppyGroupManager::ndbRefreshCompleted()
{
  RecursiveUniqueLock lock(m_mutex);

  // TODO: Verify that we have successor and predecessor

  m_fingers = m_newFingers;
  m_newFingers.clear();
}

void SloppyGroupManager::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve)
{
  RecursiveUniqueLock lock(m_mutex);

  stream << "*** Sloppy group:" << std::endl;
  stream << "Prefix length: " << m_groupPrefixLength << std::endl;
  stream << "Prefix: " << m_groupPrefix.hex() << std::endl;
  stream << "Predecessor: " << m_predecessor.nodeId.hex();
  if (resolve)
    stream << " (" << resolve(m_predecessor.nodeId) << ")";
  stream << std::endl;
  stream << "Successor: " << m_successor.nodeId.hex();
  if (resolve)
    stream << " (" << resolve(m_successor.nodeId) << ")";
  stream << std::endl;

  stream << "*** Sloppy group fingers:" << std::endl;
  for (const SloppyPeer &peer : m_fingers) {
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

  std::string localId = resolve(m_router.identity().localId());
  stream << localId << ";" << std::endl;
  stream << localId << " -> " << resolve(m_predecessor.nodeId) << ";" << std::endl;
  stream << localId << " -> " << resolve(m_successor.nodeId) << ";" << std::endl;
  for (const SloppyPeer &peer : m_fingers) {
    stream << localId << " -> " << resolve(peer.nodeId) << ";" << std::endl;
  }
}

}
