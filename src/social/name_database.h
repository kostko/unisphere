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
  NameRecord(boost::asio::io_service &service, const NodeIdentifier &nodeId);

  LandmarkAddress landmarkAddress() const;
public:
  /// Node identifier
  NodeIdentifier nodeId;
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
  NameDatabase(CompactRouter &router);

  void store(NameRecordPtr record);

  void clear();

  NameRecordPtr lookup(const NodeIdentifier &nodeId) const;
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
