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

#include "core/context.h"
#include "social/address.h"
#include "rpc/call_group.hpp"

namespace UniSphere {

class CompactRouter;
class SocialRpcChannel;

// TODO: Rename NameDatabase, etc. to LocationDatabase?

class NameRecord;
UNISPHERE_SHARED_POINTER(NameRecord)

/**
 * An entry in the name database.
 */
class UNISPHERE_EXPORT NameRecord {
public:
  /**
   * Types of name records.
   */
  enum class Type : std::uint8_t {
    /// Locally cached name record
    Cache       = 0x01,
    /// Record received via sloppy group dissemination protocol
    SloppyGroup = 0x02,
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

  /**
   * Returs true if this record is more fresh than the other record. This
   * decision is based on timestamp and seqno of both records.
   *
   * @param other The other record
   * @return True if this record is more fresh than the other
   */
  bool isMoreFresh(NameRecordPtr other) const;
public:
  /// Node identifier
  NodeIdentifier nodeId;
  /// Record type
  Type type;
  /// Current node landmark-relative addresses
  LandmarkAddressList addresses;
  /// Record liveness
  boost::posix_time::ptime lastUpdate;
  /// Expiration timer
  boost::asio::deadline_timer expiryTimer;

  /// Node that this record was received from (for records received via the
  /// sloppy group dissemination protocol)
  NodeIdentifier receivedPeerId;
  /// Originator timestamp
  std::uint32_t timestamp;
  /// Sequence number
  std::uint8_t seqno;
};

UNISPHERE_SHARED_POINTER(NameRecord)

/**
 * The name database is a central part of the routing process. It is
 * responsible for storing mappings between the location-independent
 * addresses and landmark-relative addresses.
 */
class UNISPHERE_EXPORT NameDatabase {
public:
  /// Maximum number of addresses stored in a record
  static const int max_stored_addresses = 3;
  /// Maximum number of entries in local cache
  static const int max_cache_entries = 5;

  /**
   * Class constructor.
   *
   * @param router Router instance
   */
  explicit NameDatabase(CompactRouter &router);

  NameDatabase(const NameDatabase&) = delete;
  NameDatabase &operator=(const NameDatabase&) = delete;

  /**
   * Stores a name record into the database. This method should be
   * used for locally-originating records -- it will abort when
   * attempting to save non-local sloppy group records.
   *
   * @param nodeId Destination node identifier
   * @param addresses A list of L-R addresses for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId,
             const std::list<LandmarkAddress> &addresses,
             NameRecord::Type type);

  /**
   * Stores a name record into the database. This method should be
   * used for locally-originating records -- it will abort when
   * attempting to save non-local sloppy group records.
   *
   * @param nodeId Destination node identifier
   * @param address A L-R address for this node
   * @param type Type of record
   */
  void store(const NodeIdentifier &nodeId,
             const LandmarkAddress &address,
             NameRecord::Type type);

  /**
   * Stores a foreign sloppy group record into the database. The database
   * will take ownership of the record.
   *
   * @param record Name record
   */
  void store(NameRecordPtr record);

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
   * Exports the full name database to the selected peer.
   *
   * @param peer Peer to export the routing table to
   */
  void fullUpdate(const NodeIdentifier &peer = NodeIdentifier::INVALID);

  /**
   * Returns the number of name records stored in the name database.
   */
  size_t size() const;

  /**
   * Returns the number of active (non-cache) name records in the name database.
   */
  size_t sizeActive() const;

  /**
   * Returns the number of cache name records in the name database.
   */
  size_t sizeCache() const;

  /**
   * Returns a copy of the name database of specified type. Use of this method
   * should be avoided due to copy overhead. Copying is required to ensure
   * consistency.
   *
   * @param type Name record type
   * @return A copied list of name records
   */
  std::list<NameRecordPtr> getNames(NameRecord::Type type) const;

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
private:
  UNISPHERE_DECLARE_PRIVATE(NameDatabase)
};

}

#endif
