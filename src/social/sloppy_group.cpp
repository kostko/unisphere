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

namespace UniSphere {

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

bool SloppyPeer::operator<(const SloppyPeer &other) const
{
  return nodeId < other.nodeId;
}

SloppyGroupManager::SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator)
  : m_router(router),
    m_sizeEstimator(sizeEstimator),
    m_neighborRefreshTimer(router.context().service())
{
}

void SloppyGroupManager::initialize()
{

}

void SloppyGroupManager::shutdown()
{

}

void SloppyGroupManager::refreshNeighborSet(const boost::system::error_code &error)
{
  if (error)
    return;

  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();
  NameDatabase &ndb = m_router.nameDb();
  m_oldNeighbors = m_neighbors;

  auto group = rpc.group(boost::bind(&SloppyGroupManager::ndbRefreshCompleted, this));

  // Lookup successor and predecessor
  ndb.remoteLookupClosest(m_router.identity().localId(), true, group, boost::bind(&SloppyGroupManager::ndbHandleResponse, this, _1));

  for (int i = 0; i < SloppyGroupManager::finger_count; i++) {
    NodeIdentifier fingerId;
    // TODO: Generate random finger identifier in our sloppy group
    ndb.remoteLookupClosest(fingerId, false, group, boost::bind(&SloppyGroupManager::ndbHandleResponse, this, _1));
  }
}

void SloppyGroupManager::ndbHandleResponse(const std::list<NameRecordPtr> &records)
{
  RecursiveUniqueLock lock(m_mutex);

  // TODO: LRU set
  for (NameRecordPtr record : records) {
    m_neighbors.insert(SloppyPeer(record));
  }
}

void SloppyGroupManager::ndbRefreshCompleted()
{
}

}
