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

#include <boost/asio.hpp>
#include <boost/range/any_range.hpp>

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

/// A traversable range of name record pointers
typedef boost::any_range<
  NameRecordPtr,
  boost::bidirectional_traversal_tag,
  NameRecordPtr,
  std::ptrdiff_t
> NameRecordRange;

class UNISPHERE_EXPORT NameDatabase {
public:
  /// Number of landmarks to replicate the name cache to
  static const int cache_redundancy = 3;
  /// Maximum number of addresses stored in a record
  static const int max_stored_addresses = 3;
  /// Maximum number of entries in local cache
  static const int max_cache_entries = 5;

  /**
   * Lookup types.
   */
  enum class LookupType : std::uint8_t {
    /// Return the closest name record that is not equal to the query originator
    Closest = 1,
    /// Return the left and right neighbors of the looked up identifier
    ClosestNeighbors = 2,
  };

  /**
   * Class constructor.
   *
   * @param router Router instance
   */
  explicit NameDatabase(CompactRouter &router);

  NameDatabase(const NameDatabase&) = delete;
  NameDatabase &operator=(const NameDatabase&) = delete;

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
  void store(const NodeIdentifier &nodeId,
             const std::list<LandmarkAddress> &addresses,
             NameRecord::Type type,
             const NodeIdentifier &originId = NodeIdentifier::INVALID);

  /**
   * Stores a name record into the database.
   *
   * @param nodeId Destination node identifier
   * @param address A L-R address for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId,
             const LandmarkAddress &address,
             NameRecord::Type type,
             const NodeIdentifier &originId = NodeIdentifier::INVALID);

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
                                                   size_t prefixLength,
                                                   const NodeIdentifier &origin,
                                                   LookupType type) const;

  /**
   * Performs sloppy-group related lookups on a remote node.
   *
   * @param nodeId Node identifier to look up
   * @param prefixLength Sloppy group prefix length
   * @param type Lookup type
   * @param complete Completion handler
   */
  void remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                               size_t prefixLength,
                               LookupType type,
                               std::function<void(const std::list<NameRecordPtr>&)> complete) const;

  /**
   * Performs sloppy-group related lookups on a remote node.
   *
   * @param nodeId Node identifier to look up
   * @param prefixLength Sloppy group prefix length
   * @param type Lookup type
   * @param rpcGroup RPC call group
   * @param complete Completion handler
   */
  void remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                               size_t prefixLength,
                               LookupType type,
                               RpcCallGroupPtr rpcGroup,
                               std::function<void(const std::list<NameRecordPtr>&)> complete) const;

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
   * Returns a range containing the contents of the name database.
   */
  NameRecordRange names() const;

  /**
   * Outputs the name database to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream,
            std::function<std::string(const NodeIdentifier&)> resolve = nullptr) const;
public:
  /// Signal that gets called when a name record should be exported to neighbours
  boost::signals2::signal<void(NameRecordPtr, const NodeIdentifier&)> signalExportRecord;
  /// Signal that gets called when a name record should be retracted from neighbours
  boost::signals2::signal<void(NameRecordPtr)> signalRetractRecord;
private:
  UNISPHERE_DECLARE_PRIVATE(NameDatabase)
};

}

#endif
