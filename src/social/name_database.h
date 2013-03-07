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
#ifndef UNISPHERE_SOCIAL_NAMEDATABASE_H
#define UNISPHERE_SOCIAL_NAMEDATABASE_H

#include <set>
#include <unordered_set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/asio.hpp>

#include "core/context.h"
#include "social/address.h"
#include "social/rpc_engine.h"

namespace UniSphere {

class CompactRouter;

class UNISPHERE_EXPORT NameRecord {
public:
  /**
   * Types of name records.
   */
  enum class Type : std::uint8_t {
    /// Locally cached name record
    Cache       = 0x01,
    /// Current node is landmark authority for this record
    Authority   = 0x02,
    /// Record received via sloppy group dissemination protocol
    SloppyGroup = 0x03,
  };

  /**
   * Constructs a new name record.
   *
   * @param context UNISPHERE context
   * @param nodeId Destination node identifier
   * @param type Record type
   */
  NameRecord(Context &context, const NodeIdentifier &nodeId, Type type);

  /**
   * Returns the first L-R address in this record.
   */
  LandmarkAddress landmarkAddress() const;

  /**
   * Returns the time-to-live for this record.
   */
  boost::posix_time::seconds ttl() const;

  /**
   * Returns the age of this name record.
   */
  boost::posix_time::time_duration age() const;
public:
  /// Node identifier
  NodeIdentifier nodeId;
  /// Record type
  Type type;
  /// Current node landmark-relative addresses
  std::list<LandmarkAddress> addresses;
  /// Record liveness
  boost::posix_time::ptime lastUpdate;
  /// Expiration timer
  boost::asio::deadline_timer expiryTimer;
  /// Node that this record was received from (for records received via the
  /// sloppy group dissemination protocol)
  NodeIdentifier originId;
};

UNISPHERE_SHARED_POINTER(NameRecord)

/// NIB index tags
namespace NIBTags {
  class DestinationId;
  class TypeDestination;
}

typedef boost::multi_index_container<
  NameRecordPtr,
  midx::indexed_by<
    // Index by node identifier, sorted by type
    midx::ordered_unique<
      midx::tag<NIBTags::DestinationId>,
      midx::composite_key<
        NameRecord,
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NodeIdentifier, nodeId),
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NameRecord::Type, type)
      >
    >,

    // Indey by record type and destination identifier
    midx::ordered_unique<
      midx::tag<NIBTags::TypeDestination>,
      midx::composite_key<
        NameRecord,
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NameRecord::Type, type),
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NodeIdentifier, nodeId)
      >
    >
  >
> NameInformationBase;

class UNISPHERE_EXPORT NameDatabase {
public:
  /// Number of landmarks to replicate the name cache to
  static const int cache_redundancy = 3;
  /// Maximum number of addresses stored in a record
  static const int max_stored_addresses = 3;

  /**
   * Lookup types.
   */
  enum class LookupType : std::uint8_t {
    /// Return the closest name record that is not equal to the query originator
    Closest = 1,
    /// Return the left and right neighbors of the looked up identifier
    ClosestNeighbors = 2,
  };

  NameDatabase(CompactRouter &router);

  /**
   * Initializes the name database.
   */
  void initialize();

  /**
   * Shuts down the name database.
   */
  void shutdown();

  /**
   * Stores a name record into the database.
   *
   * @param nodeId Destination node identifier
   * @param addresses A list of L-R addresses for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId, const std::list<LandmarkAddress> &addresses,
    NameRecord::Type type, const NodeIdentifier &originId = NodeIdentifier::INVALID);

  /**
   * Stores a name record into the database.
   *
   * @param nodeId Destination node identifier
   * @param address A L-R address for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId, const LandmarkAddress &address,
    NameRecord::Type type, const NodeIdentifier &originId = NodeIdentifier::INVALID);

  /**
   * Removes an existing name record from the database.
   *
   * @param nodeId Destination node identifier
   * @param type Type of record to remove
   */
  void remove(const NodeIdentifier &nodeId, NameRecord::Type type);

  /**
   * Clears the name database.
   */
  void clear();

  /**
   * Performs a local lookup of a name record.
   *
   * @param nodeId Destination node identifier
   * @return Name record pointer or null if no name record exists
   */
  const NameRecordPtr lookup(const NodeIdentifier &nodeId) const;

  /**
   * Performs sloppy-group related lookups.
   *
   * @param nodeId Node identifier to look up
   * @param prefixLength Sloppy group prefix length
   * @param origin Node that initiated the lookup
   * @param type Lookup type
   * @return Resulting name records or an empty list
   */
  const std::list<NameRecordPtr> lookupSloppyGroup(const NodeIdentifier &nodeId,
    size_t prefixLength, const NodeIdentifier &origin, LookupType type) const;

  /**
   * Performs sloppy-group related lookups on a remote node.
   *
   * @param nodeId Node identifier to look up
   * @param prefixLength Sloppy group prefix length
   * @param type Lookup type
   * @param complete Completion handler
   */
  void remoteLookupSloppyGroup(
    const NodeIdentifier &nodeId,
    size_t prefixLength,
    LookupType type,
    std::function<void(const std::list<NameRecordPtr>&)> complete
  ) const;

  /**
   * Performs sloppy-group related lookups on a remote node.
   *
   * @param nodeId Node identifier to look up
   * @param prefixLength Sloppy group prefix length
   * @param type Lookup type
   * @param rpcGroup RPC call group
   * @param complete Completion handler
   */
  void remoteLookupSloppyGroup(
    const NodeIdentifier &nodeId,
    size_t prefixLength,
    LookupType type,
    RpcCallGroupPtr rpcGroup,
    std::function<void(const std::list<NameRecordPtr>&)> complete
  ) const;

  /**
   * Registers a landmark node. This is needed for determining which landmarks
   * store which name records by the use of consistent hashing.
   *
   * @param landmarkId Landmark identifier
   */
  void registerLandmark(const NodeIdentifier &landmarkId);

  /**
   * Unregisters a landmark node. This is needed for determining which landmarks
   * store which name records by the use of consistent hashing.
   *
   * @param landmarkId Landmark identifier
   */
  void unregisterLandmark(const NodeIdentifier &landmarkId);

  /**
   * Returns a list of landmarks that are responsible for caching the given address.
   *
   * @param nodeId Destination node identifier that needs to be resolved
   * @param sgPrefixLength Sloppy group prefix length (optional); this is to return
   *   caches that contain predecessor and successor fingers
   * @return A set of landmark identifiers that should have the address
   */
  std::unordered_set<NodeIdentifier> getLandmarkCaches(const NodeIdentifier &nodeId,
    size_t sgPrefixLength = 0) const;

  /**
   * Publishes local address information to designated landmarks. This method
   * should be called when the local address changes or when the set of destination
   * landmarks change.
   */
  void publishLocalAddress();

  /**
   * Exports the full name database to the selected peer.
   *
   * @param peer Peer to export the routing table to
   */
  void fullUpdate(const NodeIdentifier &peer);

  /**
   * Returns a reference to the underlying name information base.
   */
  const NameInformationBase &getNIB() const { return m_nameDb; }

  /**
   * Outputs the name database to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve = nullptr) const;
public:
  /// Signal that gets called when a name record should be exported to neighbours
  boost::signal<void(NameRecordPtr, const NodeIdentifier&)> signalExportRecord;
  /// Signal that gets called when a name record should be retracted from neighbours
  boost::signal<void(NameRecordPtr)> signalRetractRecord;
protected:
  /**
   * Called when a record expires.
   */
  void entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record);

  /**
   * Periodically refreshes the local address.
   */
  void refreshLocalAddress(const boost::system::error_code &error);

  /**
   * Performs registration of core RPC methods that are required for name database
   * management.
   */
  void registerCoreRpcMethods();

  /**
   * Performs unregistration of core RPC methods that are required for name database
   * management.
   */
  void unregisterCoreRpcMethods();
private:
  /// Router
  CompactRouter &m_router;
  /// Mutex protecting the name database
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;
  /// Name database
  NameInformationBase m_nameDb;
  /// Bucket tree for consistent hashing
  std::set<NodeIdentifier> m_bucketTree;
  /// Landmarks that we have previously published into
  std::unordered_set<NodeIdentifier> m_publishLandmarks;
  /// Timer for periodic local address refresh
  boost::asio::deadline_timer m_localRefreshTimer;
};

}

#endif
