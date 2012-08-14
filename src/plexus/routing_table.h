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
#ifndef UNISPHERE_PLEXUS_ROUTINGTABLE_H
#define UNISPHERE_PLEXUS_ROUTINGTABLE_H

#include "core/context.h"
#include "interplex/contact.h"
#include <interplex/link.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/unordered_map.hpp>
#include <boost/signal.hpp>

namespace midx = boost::multi_index;

namespace UniSphere {

class LinkManager;

/// Bucket index type
typedef boost::uint16_t BucketIndex;

/**
 * A peer entry contains routing table information and a pointer to the
 * peer's link instance that can be used for direct communication with
 * the peer.
 */
class UNISPHERE_EXPORT PeerEntry {
public:
  /**
   * Class constructor.
   *
   * @param link Pointer to a link established with the peer node
   */
  PeerEntry(LinkPtr link);
public:
  /// Peer node identifier
  NodeIdentifier nodeId;
  /// Peer contact information
  Contact contact;
  /// Peer link
  LinkPtr link;
  
  /// Index of the bucket the peer falls into
  BucketIndex bucket;
  /// Length of the longest common prefix with local node
  size_t lcp;
  /// Distance from the local node
  NodeIdentifier distance;
  
  /// Timestamp when peer was last seen
  boost::posix_time::ptime lastSeen;
  
  // TODO Various statistics
};

// Tags for individual indices
class NodeIdTag;
class BucketIndexTag;
class BucketByDistanceTag;
class DistanceTag;
class LcpTag;

/**
 * This multi_index_container represents the peer table (= routing table)
 * with contacts organized into k-buckets. It is indexed by multiple keys for
 * fast lookups along different dimensions.
 */
typedef boost::multi_index_container<
  PeerEntry,
  midx::indexed_by<
    // Index by node identifier
    midx::ordered_unique<
      midx::tag<NodeIdTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, nodeId)
    >,
    
    // Index by entry's k-bucket
    midx::ordered_non_unique<
      midx::tag<BucketIndexTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, BucketIndex, bucket)
    >,
    
    // Index by entry's k-bucket and sorted by distance within the bucket
    midx::ordered_non_unique<
      midx::tag<BucketByDistanceTag>,
      midx::composite_key<
        PeerEntry,
        BOOST_MULTI_INDEX_MEMBER(PeerEntry, BucketIndex, bucket),
        BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, distance)
      >
    >,
    
    // Index by distance
    midx::ordered_non_unique<
      midx::tag<DistanceTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, distance)
    >,
    
    // Index by longest common prefix
    midx::ordered_non_unique<
      midx::tag<LcpTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, size_t, lcp)
    >
  >
> PeerTable;

/// A list of neighbor entries
typedef std::list<PeerEntry> PeerEntryList;

/**
 * A class that manages the Kademlia routing table for the UNISPHERE
 * overlay.
 */
class UNISPHERE_EXPORT RoutingTable {
public:
  /**
   * Class constructor.
   *
   * @param manager Link manager instance
   * @param bucketSize Maximum bucket size
   */
  RoutingTable(LinkManager &manager, size_t bucketSize);
  
  /**
   * Adds a new entry into the routing table.
   *
   * @param link Link to the peer in question
   * @return True if routing table has been changed
   */
  bool add(LinkPtr link);
  
  /**
   * Returns a number of contacts closest to the destination.
   *
   * @param destination Destination identifier
   * @param count Maximum number of contacts to return
   * @return A list of closest neighbor entries
   */
  PeerEntryList lookup(const NodeIdentifier &destination, size_t count);
  
  /**
   * Returns a maximum of @ref count contacts that are in the same
   * bucket as the @ref destination.
   *
   * @param destination Destination identifier
   * @param count Maximum number of contacts to return
   * @param includeTarget Should the target contact be included
   * @return A list of neighbor entries
   */
  PeerEntryList lookupInBucket(const NodeIdentifier &destination, size_t count,
    bool includeTarget = true);
  
  /**
   * Samples @ref count entries from each bucket in the common prefix with
   * target identifier.
   *
   * @param target Target identifier
   * @param count Number of entries to sample per bucket
   * @return A list of sampled entries
   */
  PeerEntryList sampleFromBuckets(const NodeIdentifier &target, size_t count);
  
  /**
   * Returns contact information from the routing table. In case no
   * contact information is available, returns a null contact.
   *
   * @param nodeId Destination node identifier
   * @return Contact for the target node
   */
  Contact get(const NodeIdentifier &nodeId) const;
  
  /**
   * Returns the number of buckets currently in the routing table.
   */
  size_t bucketCount() const { return m_localBucket + 1; }
  
  /**
   * Returns the number of neighbor entries currently in the routing table.
   */
  size_t neighborCount() const;
public:
  // Signals
  // TODO signal for pinging entries while adding into full buckets
protected:
  /**
   * A helper method that returns a bucket index for the given node
   * identifier.
   *
   * @param id Node identifier
   * @return A valid bucket index
   */
  BucketIndex bucketForIdentifier(const NodeIdentifier &id) const;
  
  /**
   * Returns the number of entries in the given bucket.
   *
   * @param bucket Bucket index
   * @return Number of entries in the bucket
   */
  size_t getBucketSize(BucketIndex bucket) const;
  
  /**
   * Performs the actual insertion of a node into the routing table. This
   * method is called by add with a shared lock held in read mode.
   *
   * @param link Link to the peer in question
   * @param lock Shared pointer to upgradable lock
   * @return True if routing table has been changed
   */
  bool insert(LinkPtr link, UpgradableLockPtr lock);
  
  /**
   * Splits the current local bucket into two buckets. The new local bucket
   * index will be for one larger than the old one.
   *
   * @param lock Upgradable lock that will be used when doing modifications
   * @return True when split has been successful, false when table is full
   */
  bool split(UpgradableLockPtr lock);
private:
  /// Link manager associated with this router
  LinkManager &m_manager;
  
  /// Local node identifier (obtained from link manager)
  NodeIdentifier m_localId;
  /// Bucket where local node identifier is located in
  BucketIndex m_localBucket;
  
  /// The routing table
  PeerTable m_peers;
  /// Mutex protecting changes to the routing table
  boost::shared_mutex m_mutex;
  /// Maximum number of entries in a bucket
  size_t m_maxBucketSize;
  /// Maximum number of buckets
  size_t m_maxBuckets;
};

UNISPHERE_SHARED_POINTER(RoutingTable)

}

#endif
