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
   * Constructs an invalid peer entry.
   */
  PeerEntry();
  
  /**
   * Class constructor.
   * 
   * @param nodeId Peer node identifier
   */
  PeerEntry(const NodeIdentifier &nodeId);
  
  /**
   * Class constructor.
   *
   * @param link Pointer to a link established with the peer node
   */
  PeerEntry(LinkPtr link);
  
  /**
   * Returns true if this peer entry has a valid node identifier
   * and link associated with it.
   */
  bool isValid() const { return nodeId.isValid() && link; }
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

/**
 * Siblings are nodes that are responsible for a specific key in the DHT. This
 * table contains peer entries that constitute the sibling neighbourhood of
 * the local node.
 */
typedef boost::multi_index_container<
  PeerEntry,
  midx::indexed_by<
    // Index by node identifier
    midx::ordered_unique<
      midx::tag<NodeIdTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, nodeId)
    >,
    
    // Index by distance
    midx::ordered_non_unique<
      midx::tag<DistanceTag>,
      BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, distance)
    >
  >
> SiblingTable;

/**
 * A class that orders stores peer entries in a multi index container and only
 * keeps a specified number of closest ones by XOR distance metric.
 */
class UNISPHERE_EXPORT DistanceOrderedTable {
public:
  /// Container type that sorts by distance and node identifiers
  typedef boost::multi_index_container<
    PeerEntry,
    midx::indexed_by<
      // Index by node identifier
      midx::ordered_unique<
        midx::tag<NodeIdTag>,
        BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, nodeId)
      >,
      
      // Index by distance
      midx::ordered_non_unique<
        midx::tag<DistanceTag>,
        BOOST_MULTI_INDEX_MEMBER(PeerEntry, NodeIdentifier, distance)
      >
    >
  > DistanceTable;
  
  /**
   * Class constructor.
   * 
   * @param key Key to measure distance to
   * @param maxSize Maximum number of top entries to keep
   */
  DistanceOrderedTable(const NodeIdentifier &key, size_t maxSize);
  
  /**
   * Inserts a new peer entry into the table.
   * 
   * @param entry Peer entry
   */
  void insert(const PeerEntry &entry);
  
  /**
   * Returns the underlying multi index container.
   */
  inline DistanceTable &table() { return m_table; }
private:
  /// Key to measure distance to
  NodeIdentifier m_key;
  /// Maximum number of peer entries to keep
  size_t m_maxSize;
  /// The underlying distance table
  DistanceTable m_table;
};

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
   * @param numSiblings Number of siblings
   */
  RoutingTable(LinkManager &manager, size_t bucketSize, size_t numSiblings);
  
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
  DistanceOrderedTable lookup(const NodeIdentifier &destination, size_t count);
  
  bool isSiblingFor(const NodeIdentifier &node, const NodeIdentifier &key);
  
  /**
   * Returns the routing table entry for a specific node.
   *
   * @param nodeId Destination node identifier
   * @return Routing entry for the specified node
   */
  PeerEntry get(const NodeIdentifier &nodeId);
  
  /**
   * Returns the number of buckets currently in the routing table.
   */
  size_t bucketCount() const { return m_localBucket + 1; }
  
  /**
   * Returns the number of peer entries currently in the routing table
   * without counting the sibling table.
   */
  size_t peerCount() const;
  
  /**
   * Returns the number of sibling entries currently in the sibling table.
   */
  size_t siblingCount() const;
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
   * @param entry Peer entry to insert
   * @param lock Shared pointer to upgradable lock
   * @return True if routing table has been changed
   */
  bool insert(PeerEntry &entry, UpgradableLockPtr lock);
  
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
  
  /// The sibling table
  SiblingTable m_siblings;
  /// Number of siblings for each key
  size_t m_numKeySiblings;
  /// Maximum number of entries in the sibling table
  size_t m_maxSiblingsSize;
};

UNISPHERE_SHARED_POINTER(RoutingTable)

}

#endif
