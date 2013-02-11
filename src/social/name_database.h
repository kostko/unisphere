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

#include <unordered_map>
#include <boost/asio.hpp>

#include "core/globals.h"
#include "social/address.h"

namespace UniSphere {

class CompactRouter;

class NameRecord {
public:
  enum class Type : std::uint8_t {
    Cache       = 0x01,
    SloppyGroup = 0x02,
  };

  NameRecord(boost::asio::io_service &service, const NodeIdentifier &nodeId,
    Type type);

  LandmarkAddress landmarkAddress() const;

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

class NameDatabase {
public:
  /// Number of landmarks to replicate the name cache to
  static const int cache_redundancy = 3;

  NameDatabase(CompactRouter &router);

  void store(const NodeIdentifier &nodeId, const LandmarkAddress &address,
    NameRecord::Type type);

  void remove(const NodeIdentifier &nodeId);

  void clear();

  NameRecordPtr lookup(const NodeIdentifier &nodeId) const;

  void registerLandmark(const NodeIdentifier &landmarkId);

  void unregisterLandmark(const NodeIdentifier &landmarkId);

  /**
   * Returns a list of landmarks that are responsible for caching the given address.
   *
   * @param nodeId Destination node identifier that needs to be resolved
   * @return A list of landmark identifiers that should have the address
   */
  std::list<NodeIdentifier> getLandmarkCaches(const NodeIdentifier &nodeId) const;
protected:
  void entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record);
private:
  /// Router
  CompactRouter &m_router;
  /// Mutex protecting the name database
  mutable std::recursive_mutex m_mutex;
  /// Name database (populated only if local node is a landmark)
  std::unordered_map<NodeIdentifier, NameRecordPtr> m_nameDb;
};

}

#endif
