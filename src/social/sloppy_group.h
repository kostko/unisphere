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
  SloppyPeer();

  explicit SloppyPeer(const NodeIdentifier &nodeId);

  SloppyPeer(const NodeIdentifier &nodeId, const LandmarkAddress &address);

  /**
   * Constructs a sloppy peer from a name record.
   *
   * @param record Name record pointer
   */
  explicit SloppyPeer(NameRecordPtr record);

  bool isNull() const { return nodeId.isNull(); }

  void clear();

  LandmarkAddress landmarkAddress() const;

  bool operator<(const SloppyPeer &other) const
  {
    return nodeId < other.nodeId;
  }

  bool operator>(const SloppyPeer &other) const
  {
    return nodeId > other.nodeId;
  }

  bool operator==(const SloppyPeer &other) const
  {
    return nodeId == other.nodeId;
  }
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
  /// Announce interval
  static const int interval_announce = 600;

  /**
   * Sloppy group message types.
   */
  enum class MessageType : std::uint8_t {
    /// Name announce propagated via DV-like protocol
    NameAnnounce  = 0x01,
    /// Finger reject
    FingerReject  = 0x02
  };

  SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  SloppyGroupManager(const SloppyGroupManager&) = delete;
  SloppyGroupManager &operator=(const SloppyGroupManager&) = delete;

  void initialize();

  void shutdown();

  /**
   * Outputs the sloppy group to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve = nullptr);

  void dumpTopology(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve = nullptr);
protected:
  void refreshNeighborSet(const boost::system::error_code &error);

  void refreshOneLongFinger(std::function<void(const std::list<NameRecordPtr>&, const NodeIdentifier&)> handler,
    RpcCallGroupPtr rpcGroup = RpcCallGroupPtr());

  NameRecordPtr filterLongFingers(const std::list<NameRecordPtr> &records,
    const NodeIdentifier &referenceId);

  void ndbHandleResponseShort(const std::list<NameRecordPtr> &records);

  void ndbHandleResponseLong(const std::list<NameRecordPtr> &records, const NodeIdentifier &targetId);

  void ndbRefreshCompleted();

  /**
   * Announces local sloppy group name records to the neighbor set.
   */
  void announceFullRecords(const boost::system::error_code &error);

  void nibExportRecord(NameRecordPtr record, const NodeIdentifier &peerId);

  void networkSizeEstimateChanged(std::uint64_t size);

  void rejectPeerLink(const RoutedMessage &msg);

  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const RoutedMessage &msg);
private:
  class BlacklistedPeer {
  public:
    explicit BlacklistedPeer(const NodeIdentifier &peerId)
      : peerId(peerId)
    {
    }

    BlacklistedPeer(Context &context, const NodeIdentifier &peerId)
      : peerId(peerId),
        timer(new boost::asio::deadline_timer(context.service()))
    {
    }

    bool operator<(const BlacklistedPeer &other) const
    {
      return peerId < other.peerId;
    }

    bool operator==(const BlacklistedPeer &other) const
    {
      return peerId == other.peerId;
    }
  public:
    NodeIdentifier peerId;
    boost::shared_ptr<boost::asio::deadline_timer> timer;
  };

  /// Router instance
  CompactRouter &m_router;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Name database reference
  NameDatabase &m_nameDb;
  /// Mutex protecting the sloppy group manager
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;
  /// Predecessor in the overlay
  SloppyPeer m_predecessor;
  /// Successor in the overlay
  SloppyPeer m_successor;
  /// Outgoing fingers (including predecessor and successor)
  std::map<NodeIdentifier, SloppyPeer> m_fingersOut;
  /// Incoming fingers
  std::map<NodeIdentifier, SloppyPeer> m_fingersIn;
  /// The set of newly discovered short fingers
  std::set<SloppyPeer> m_newShortFingers;
  /// The set of newly discovered long fingers
  std::map<NodeIdentifier, std::list<NameRecordPtr>> m_newLongFingers;
  /// A list of peers that should be temporarily excluded from becoming fingers
  std::set<BlacklistedPeer> m_blacklistedPeers;
  /// Timer for periodic neighbor set refresh
  boost::asio::deadline_timer m_neighborRefreshTimer;
  /// Timer for periodic annouces
  boost::asio::deadline_timer m_announceTimer;
  /// Active subscriptions to other components
  std::list<boost::signals::connection> m_subscriptions;
  /// Sloppy group prefix length
  size_t m_groupPrefixLength;
  /// Sloppy group prefix
  NodeIdentifier m_groupPrefix;
  /// Expected sloppy group size
  double m_groupSize;
};

}

#endif
