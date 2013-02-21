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
#ifndef UNISPHERE_SOCIAL_SLOPPYGROUP_H
#define UNISPHERE_SOCIAL_SLOPPYGROUP_H

#include <set>
#include <boost/asio.hpp>

#include "identity/node_identifier.h"
#include "social/address.h"
#include "social/name_database.h"

namespace UniSphere {

class CompactRouter;
class NetworkSizeEstimator;

class UNISPHERE_EXPORT SloppyPeer {
public:
  /**
   * Constructs a sloppy peer from a name record.
   *
   * @param record Name record pointer
   */
  explicit SloppyPeer(NameRecordPtr record);

  LandmarkAddress landmarkAddress() const;

  bool operator<(const SloppyPeer &other) const;
public:
  /// Sloppy peer node identifier
  NodeIdentifier nodeId;
  /// A list of L-R addresses for this sloppy peer
  std::list<LandmarkAddress> addresses;
};

/**
 * Represents the sloppy group overlay manager.
 */
class UNISPHERE_EXPORT SloppyGroupManager {
public:
  /// Number of long-distance fingers to establish
  static const int finger_count = 1;

  SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  void initialize();

  void shutdown();
protected:
  void refreshNeighborSet(const boost::system::error_code &error);

  void ndbHandleResponse(const std::list<NameRecordPtr> &records);

  void ndbRefreshCompleted();
private:
  /// Router instance
  CompactRouter &m_router;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Mutex protecting the sloppy group manager
  mutable std::recursive_mutex m_mutex;
  /// Neighbors in the overlay
  std::set<SloppyPeer> m_neighbors;
  /// Neighbors in the overlay before last refresh
  std::set<SloppyPeer> m_oldNeighbors;
  /// Timer for periodic neighbor set refresh
  boost::asio::deadline_timer m_neighborRefreshTimer;
};

}

#endif
