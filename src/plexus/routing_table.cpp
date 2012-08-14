/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2011 Jernej Kos <kostko@unimatrix-one.org>
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
#include "plexus/routing_table.h"
#include "interplex/link_manager.h"

#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>

namespace UniSphere {

PeerEntry::PeerEntry(LinkPtr link)
  : nodeId(link->nodeId()),
    contact(link->contact()),
    link(link),
    bucket(0),
    lastSeen(boost::posix_time::microsec_clock::universal_time())
{
}

RoutingTable::RoutingTable(LinkManager &manager, size_t bucketSize)
  : m_manager(manager),
    m_localId(manager.getLocalNodeId()),
    m_localBucket(0),
    m_maxBucketSize(bucketSize),
    m_maxBuckets(NodeIdentifier::length * 8)
{
}

BucketIndex RoutingTable::bucketForIdentifier(const NodeIdentifier &id) const
{
  BucketIndex bucket = id.longestCommonPrefix(m_localId);
  if (bucket > m_localBucket)
    bucket = m_localBucket;
  
  return bucket;
}

size_t RoutingTable::getBucketSize(BucketIndex bucket) const
{
  auto entries = m_peers.get<BucketIndexTag>().equal_range(bucket);
  return std::distance(entries.first, entries.second);
}

size_t RoutingTable::neighborCount() const
{
  return m_peers.size();
}

bool RoutingTable::add(LinkPtr link)
{
  UpgradableLockPtr lock(new UpgradableLock(m_mutex));
  return insert(link, lock);
}

bool RoutingTable::insert(LinkPtr link, UpgradableLockPtr lock)
{
  BOOST_ASSERT(link && !link->contact().isNull());
  
  // First check if this entry is already present in the neighbor table and update
  // contact information if so
  auto existing = m_peers.get<NodeIdTag>().find(link->nodeId());
  if (existing != m_peers.end()) {
    m_peers.modify(existing, [&](PeerEntry &entry) {
      entry.link = link;
      entry.lastSeen = boost::posix_time::microsec_clock::universal_time(); 
    });
    return false;
  }
  
  // Check if destination bucket can accommodate a new host
  BucketIndex bucket = bucketForIdentifier(link->nodeId());
  if (getBucketSize(bucket) < m_maxBucketSize) {
    // Bucket is free so we can insert without any problems
    PeerEntry entry(link);
    entry.bucket = bucket;
    entry.lcp = link->nodeId().longestCommonPrefix(m_localId);
    entry.distance = link->nodeId() ^ m_localId;
    
    // Upgrade to exclusive access lock and insert the entry
    UpgradeToUniqueLock unique(*lock);
    m_peers.insert(entry);
    return true;
  } else if (bucket == m_localBucket) {
    // New host is in local bucket and it is full, we can split it and then
    // retry the insertion
    if (split(lock)) {
      return insert(link, lock);
    }
  } else {
    // New host might be among the k-neighborhood and we might insert into
    // some sibling bucket; there are two options where we could insert after
    // the target bucket has been deemed full:
    //     1) The immediate sibling (m_localBucket - 1) may be full and
    //        contain the k-neighborhood.
    //
    //     2) K-neighborhood may also be spread among multiple buckets
    //        but the only one that matters is the lowest one (the least
    //        amount of common bits to local identifier), since we can only
    //        be here because of a full bucket and therefore intermediate
    //        buckets can't be full or the lowest one would not be in
    //        k-neighborhood (there can't be more than k entries in k-neigh.)
    PeerEntryList closest = lookup(m_localId, m_maxBucketSize);
    if (bucket == m_localBucket - 1 || bucket == closest.back().bucket) {
      // Calculate number of k-neighborhood in non-local bucket
      if (m_maxBucketSize - getBucketSize(m_localBucket) > 0) {
        // Check that target identifier is really among the closest
        auto others = m_peers.get<BucketByDistanceTag>().equal_range(
          boost::make_tuple(bucket));
        auto last = --others.second;
        
        NodeIdentifier distance = m_localId ^ link->nodeId();
        if (distance < (*last).distance) {
          // New contact is within the k-neighborhood, replace existing
          PeerEntry entry(link);
          entry.bucket = bucket;
          entry.lcp = link->nodeId().longestCommonPrefix(m_localId);
          entry.distance = distance;
          
          UpgradeToUniqueLock unique(*lock);
          m_peers.erase(m_peers.get<NodeIdTag>().find((*last).nodeId));
          m_peers.insert(entry);
          return true;
        }
      }
    }
    
    // We are not inserting into a sibling bucket; check for any bad contacts
    // and replace them
    /*auto candidates = m_peers.get<BucketIndexTag>().equal_range(bucket);
    for (auto it = candidates.first; it != candidates.second; it++) {
      if ((*it).bad) {
        // There is a bad contact that we can replace
        PeerEntry entry(link);
        entry.bucket = bucket;
        entry.lcp = link->nodeId().longestCommonPrefix(m_localId);
        entry.distance = link->nodeId() ^ m_localId;
        
        UpgradeToUniqueLock unique(*lock);
        m_neighbors.erase(m_neighbors.get<NodeIdTag>().find((*it).nodeId));
        m_neighbors.insert(entry);
        return true;
      }
    }*/
  }
  
  return false;
}

bool RoutingTable::split(UpgradableLockPtr lock)
{
  if (m_localBucket >= m_maxBuckets)
    return false;
  
  // Split local bucket into two buckets
  auto entries = m_peers.get<BucketIndexTag>().equal_range(m_localBucket);
  std::list<NodeIdentifier> candidates;
  
  for (auto it = entries.first; it != entries.second; ++it) {
    if ((*it).lcp > m_localBucket)
      candidates.push_back((*it).nodeId);
  }
  
  // Do the actual changes here, otherwise we would loose our initial iterators
  auto &nodes = m_peers.get<NodeIdTag>();
  UpgradeToUniqueLock unique(*lock);
  BOOST_FOREACH(const NodeIdentifier &nodeId, candidates) {
    nodes.modify(
      nodes.find(nodeId), [=](PeerEntry &entry) { entry.bucket = m_localBucket; }
    );
  }
  
  // Local bucket is now the new one
  m_localBucket++;
  return true;
}

PeerEntryList RoutingTable::lookupInBucket(const NodeIdentifier &destination, size_t count,
  bool includeTarget)
{
  SharedLock lock(m_mutex);
  
  // This is much simpler as lookup, since we just fetch entries for the target bucket
  BucketIndex bucket = bucketForIdentifier(destination);
  PeerEntryList result;
  auto entries = m_peers.get<BucketByDistanceTag>().equal_range(boost::make_tuple(bucket));
  for (auto it = entries.first; it != entries.second && count > 0; it++) {
    if (!includeTarget && (*it).nodeId == destination)
      continue;
    
    result.push_back((*it));
    count--;
  }
  
  return result;
}

PeerEntryList RoutingTable::sampleFromBuckets(const NodeIdentifier &target, size_t count)
{
  SharedLock lock(m_mutex);
  
  // Fetch destination bucket and sample all buckets up to it
  PeerEntryList result;
  size_t currentCount = 0;
  size_t lastLcp = 0;
  size_t lcp = target.longestCommonPrefix(m_localId);
  auto entries = m_peers.get<LcpTag>().range(0 <= boost::lambda::_1, boost::lambda::_1 < lcp);
  for (auto it = entries.first; it != entries.second; it++) {
    if ((*it).nodeId == target)
      continue;
    if ((*it).lcp != lastLcp)
      currentCount = 0;
    
    if (++currentCount > count)
      continue;
    result.push_back(*it);
  }
  
  return result;
}

PeerEntryList RoutingTable::lookup(const NodeIdentifier &destination, size_t count)
{
  SharedLock lock(m_mutex);
  
  // Sample enough buckets around the destination bucket so we will be able to get at least
  // count entries if there are so many available
  BucketIndex startBucket = bucketForIdentifier(destination);
  BucketIndex endBucket = startBucket + 1;
  
  size_t actualSize = getBucketSize(startBucket);
  
  // Add all buckets with more bits in common and if that is still not enough, add buckets
  // with less bits in common
  while (endBucket <= m_localBucket) { actualSize += getBucketSize(endBucket++); }
  while (actualSize < count && startBucket > 0) { actualSize += getBucketSize(--startBucket); }
  
  // Now put all contacts into a container and sort by distance
  auto entries = m_peers.get<BucketIndexTag>().range(
    startBucket <= boost::lambda::_1,
    boost::lambda::_1 <= endBucket
  );
  
  boost::multi_index_container<
    PeerEntry,
    midx::indexed_by<
      // Index by distance
      midx::ordered_non_unique<
        midx::tag<DistanceTag>,
        BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, distance)
      >
    >
  > candidates;

  for (auto it = entries.first; it != entries.second; it++) {
    PeerEntry entry = *it;
    entry.distance = (*it).contact.nodeId() ^ destination;
    candidates.insert(entry);
  }
  
  // Select the closest count entries
  PeerEntryList result;
  BOOST_FOREACH(const PeerEntry &entry, candidates) {
    result.push_back(entry);
    if (--count <= 0)
      break;
  }
  
  return result;
}

Contact RoutingTable::get(const NodeIdentifier &nodeId) const
{
  auto node = m_peers.get<NodeIdTag>().find(nodeId);
  if (node == m_peers.end())
    return Contact();
  
  return (*node).contact;
}

}
