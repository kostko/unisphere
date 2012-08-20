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

#include <boost/foreach.hpp>
#include <boost/lambda/lambda.hpp>

namespace UniSphere {

PeerEntry::PeerEntry()
{
}

PeerEntry::PeerEntry (const NodeIdentifier &nodeId)
  : nodeId(nodeId),
    bucket(0),
    lastSeen(boost::posix_time::microsec_clock::universal_time())
{
}
  
PeerEntry::PeerEntry(LinkPtr link)
  : nodeId(link->nodeId()),
    contact(link->contact()),
    link(link),
    bucket(0),
    lastSeen(boost::posix_time::microsec_clock::universal_time())
{
}

DistanceOrderedTable::DistanceOrderedTable(const NodeIdentifier &key, size_t maxSize)
  : m_key(key),
    m_maxSize(maxSize)
{
}

void DistanceOrderedTable::insert(const PeerEntry &entry)
{
  PeerEntry e = entry;
  e.distance = e.nodeId ^ m_key;
  m_table.insert(e);
  
  // When there are already too many entries, remove the most distant one
  if (m_table.size() > m_maxSize) {
    auto &d = m_table.get<DistanceTag>();
    d.erase(--d.end());
  }
}

RoutingTable::RoutingTable(const NodeIdentifier &localId, size_t bucketSize, size_t numSiblings)
  : m_localId(localId),
    m_localBucket(0),
    m_maxBucketSize(bucketSize),
    m_maxBuckets(NodeIdentifier::length * 8),
    m_numKeySiblings(numSiblings),
    // Multiplier set to five due to proof in S/Kademlia paper
    m_maxSiblingsSize(5 * numSiblings)
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
  return m_peers.get<BucketIndexTag>().count(bucket);
}

size_t RoutingTable::peerCount() const
{
  return m_peers.size();
}

size_t RoutingTable::siblingCount() const
{
  return m_siblings.size();
}

bool RoutingTable::add(LinkPtr link)
{
  RecursiveUniqueLock lock(m_mutex);
  
  // Generate a peer entry that might be added to the routing tables
  PeerEntry entry(link);
  BOOST_ASSERT(entry.isValid());
  entry.lcp = link->nodeId().longestCommonPrefix(m_localId);
  entry.distance = link->nodeId() ^ m_localId;
  return insert(entry);
}

bool RoutingTable::add(const NodeIdentifier &nodeId)
{
  RecursiveUniqueLock lock(m_mutex);
  
  // Generate a peer entry that might be added to the routing tables
  PeerEntry entry(nodeId);
  entry.lcp = nodeId.longestCommonPrefix(m_localId);
  entry.distance = nodeId ^ m_localId;
  return insert(entry);
}

bool RoutingTable::insert(PeerEntry &entry)
{
  RecursiveUniqueLock lock(m_mutex);
  BOOST_ASSERT(entry.nodeId != m_localId);
  
  // Check if node is already a sibling
  auto sibling = m_siblings.get<NodeIdTag>().find(entry.nodeId);
  if (sibling != m_siblings.end()) {
    m_siblings.modify(sibling, [&](PeerEntry &e) {
      e.link = entry.link;
      e.lastSeen = boost::posix_time::microsec_clock::universal_time();
    });
    return false;
  }
  
  // First check if this entry is already present in the neighbor table and update
  // contact information if so
  auto existing = m_peers.get<NodeIdTag>().find(entry.nodeId);
  if (existing != m_peers.end()) {
    m_peers.modify(existing, [&](PeerEntry &e) {
      e.link = entry.link;
      e.lastSeen = boost::posix_time::microsec_clock::universal_time(); 
    });
    return false;
  }
  
  // Generate a peer entry that might be added to the routing tables
  bool result = false;
  
  // Check if this entry can be added to the sibling list (the new identifier
  // falls somewhere between existing identifiers in the sibling list)
  auto &sibs = m_siblings.get<DistanceTag>();
  if (m_siblings.size() != m_maxSiblingsSize || entry.distance < (*sibs.rbegin()).distance) {
    if (m_siblings.size() == m_maxSiblingsSize) {
      // Sibling list is already full, the most distant entry shall be removed (and
      // possibly moved to the k-buckets)
      PeerEntry oldSibling = *sibs.rbegin();
      sibs.erase(--sibs.end());
      m_siblings.insert(PeerEntry(entry));
      
      entry = oldSibling;
      result = true;
    } else {
      // Simply add to the sibling list
      m_siblings.insert(entry);
      return true;
    }
  }
  
  // Check if destination bucket can accommodate a new host
  BucketIndex bucket = bucketForIdentifier(entry.nodeId);
  if (getBucketSize(bucket) < m_maxBucketSize) {
    // Bucket is free so we can insert without any problems
    entry.bucket = bucket;
    m_peers.insert(entry);
    return true;
  } else if (bucket == m_localBucket) {
    // New host is in local bucket and it is full, we can split it and then
    // retry the insertion
    if (split()) {
      return insert(entry);
    }
  } else {
    // Check if we can replace some existing entry in the target bucket
    auto candidates = m_peers.get<BucketIndexTag>().equal_range(bucket);
    for (auto it = candidates.first; it != candidates.second; it++) {
      // TODO Perform selection based on priority and deadness
    }
  }
  
  return result;
}

bool RoutingTable::split()
{
  RecursiveUniqueLock lock(m_mutex);
  
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
  BOOST_FOREACH(const NodeIdentifier &nodeId, candidates) {
    nodes.modify(
      nodes.find(nodeId), [=](PeerEntry &entry) { entry.bucket = m_localBucket; }
    );
  }
  
  // Local bucket is now the new one
  m_localBucket++;
  return true;
}

bool RoutingTable::isSiblingFor(const NodeIdentifier &node, const NodeIdentifier &key)
{
  RecursiveUniqueLock lock(m_mutex);
  
  // First check that the specified key is inside the sibling neighbourhood
  // of the local node; if not, we can't determine sibling status
  if (m_siblings.size() == m_maxSiblingsSize) {
    PeerEntry edgeNode = *m_siblings.get<DistanceTag>().rbegin();
    
    // If distance to key is greater than distance to the most distant node
    // in the sibling list, the key is outside radius so we can't know anything
    if ((m_localId ^ key) > edgeNode.distance)
      return false;
  }
  
  // Order potential siblings by their distance to target key
  DistanceOrderedTable candidates(key, m_numKeySiblings);
  auto &entries = m_siblings.get<NodeIdTag>();
  for (auto it = entries.begin(); it != entries.end(); it++) {
    candidates.insert(*it);
  }
  
  // Insert the local node as it is also a candidate
  candidates.insert(PeerEntry(m_localId));
  
  // Check if specified node is among the sibling candidates
  return candidates.table().get<NodeIdTag>().find(node) != candidates.table().end();
}

DistanceOrderedTable RoutingTable::lookup(const NodeIdentifier &destination, size_t count)
{
  RecursiveUniqueLock lock(m_mutex);
  DistanceOrderedTable result(destination, count);
  
  // If there are no siblings, we can only deliver to the local node
  if (!m_siblings.size()) {
    result.insert(PeerEntry(m_localId));
    return result;
  }
  
  // Sample enough buckets around the destination bucket so we will be able to get at least
  // count entries if there are so many available
  BucketIndex startBucket = bucketForIdentifier(destination);
  BucketIndex endBucket = startBucket + 1;
  
  size_t actualSize = getBucketSize(startBucket);
  
  // Add all buckets with more bits in common and if that is still not enough, add buckets
  // with less bits in common
  while (endBucket <= m_localBucket) { actualSize += getBucketSize(endBucket++); }
  while (actualSize < count && startBucket > 0) { actualSize += getBucketSize(--startBucket); }
  
  // If this node is a sibling, we should also consider all entries in the sibling table; also
  // if we have sampled all buckets and still don't have enough entries to return
  if (isSiblingFor(m_localId, destination) || actualSize < count) {
    actualSize += m_siblings.size();
    BOOST_FOREACH(const PeerEntry &entry, m_siblings) {
      result.insert(entry);
    }
  }
  
  // Now put all contacts into a container and sort by distance
  auto entries = m_peers.get<BucketIndexTag>().range(
    startBucket <= boost::lambda::_1,
    boost::lambda::_1 <= endBucket
  );
  
  for (auto it = entries.first; it != entries.second; it++) {
    result.insert(*it);
  }
  return result;
}

bool RoutingTable::remove(const NodeIdentifier &nodeId)
{
  RecursiveUniqueLock lock(m_mutex);
  
  // Check if the entry is a sibling entry and remove it
  auto sibling = m_siblings.get<NodeIdTag>().find(nodeId);
  if (sibling != m_siblings.end()) {
    m_siblings.erase(sibling);
    
    // If there are no more siblings left, this means that the whole routing table
    // has become empty; in this case we need to rejoin the overlay
    if (!m_siblings.size()) {
      lock.unlock();
      signalRejoin();
      return true;
    }
    
    // Attempt to refill the missing sibling position
    refillSiblingTable();
    return true;
  }
  
  // Check if entry is in one of the buckets and remove it
  auto peer = m_peers.get<NodeIdTag>().find(nodeId);
  if (peer != m_peers.end()) {
    m_peers.erase(peer);
    
    // TODO Implement replacement cache?
    
    return true;
  }
  
  return false;
}

void RoutingTable::refillSiblingTable()
{
  RecursiveUniqueLock lock(m_mutex);
  
  // If the sibling table is already full or empty we have nothing to do
  if (m_siblings.size() == m_maxSiblingsSize || !m_siblings.size())
    return;
  
  // Find the closest k-bucket with at least one entry in it
  BucketIndex bucket = m_localBucket;
  while (bucket > 0 && getBucketSize(bucket) == 0) { bucket--; }
  
  // Select the closest peer from this bucket and move it to the sibling table
  auto &bd = m_peers.get<BucketByDistanceTag>();
  auto peers = bd.equal_range(boost::make_tuple(bucket));
  m_siblings.insert(*peers.first);
  bd.erase(peers.first);
}

PeerEntry RoutingTable::get(const NodeIdentifier &nodeId)
{
  RecursiveUniqueLock lock(m_mutex);
  auto node = m_peers.get<NodeIdTag>().find(nodeId);
  if (node == m_peers.end())
    return PeerEntry();
  
  return *node;
}

}
