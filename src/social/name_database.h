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

#include <map>
#include <unordered_set>
#include <boost/asio.hpp>

#include "core/context.h"
#include "social/address.h"

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
};

UNISPHERE_SHARED_POINTER(NameRecord)

class UNISPHERE_EXPORT NameDatabase {
public:
  /// Number of landmarks to replicate the name cache to
  static const int cache_redundancy = 3;
  /// Maximum number of addresses stored in a record
  static const int max_stored_addresses = 3;

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
    NameRecord::Type type);

  /**
   * Stores a name record into the database.
   *
   * @param nodeId Destination node identifier
   * @param address A L-R address for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId, const LandmarkAddress &address,
    NameRecord::Type type);

  /**
   * Removes an existing name record from the database.
   *
   * @param nodeId Destination node identifier
   */
  void remove(const NodeIdentifier &nodeId);

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
  NameRecordPtr lookup(const NodeIdentifier &nodeId) const;

  /**
   * Looks up the closest node identifier to the one given.
   *
   * @param nodeId Destination node identifier
   * @return Name record pointer or null if the database is empty
   */
  NameRecordPtr lookupClosest(const NodeIdentifier &nodeId) const;

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
   * @return A set of landmark identifiers that should have the address
   */
  std::unordered_set<NodeIdentifier> getLandmarkCaches(const NodeIdentifier &nodeId) const;

  /**
   * Publishes local address information to designated landmarks. This method
   * should be called when the local address changes or when the set of destination
   * landmarks change.
   */
  void publishLocalAddress();

  /**
   * Outputs the name database to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve = nullptr);
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
   * Hashes a node identifier for use in consistent hashing.
   *
   * @param nodeId Node identifier
   * @return Hashed node identifier
   */
  std::string hashIdentifier(const NodeIdentifier &nodeId) const;

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
  /// Name database
  std::map<NodeIdentifier, NameRecordPtr> m_nameDb;
  /// Bucket tree for consistent hashing
  std::map<std::string, NodeIdentifier> m_bucketTree;
  /// Landmarks that we have previously published into
  std::unordered_set<NodeIdentifier> m_publishLandmarks;
  /// Timer for periodic local address refresh
  boost::asio::deadline_timer m_localRefreshTimer;
};

}

#endif
