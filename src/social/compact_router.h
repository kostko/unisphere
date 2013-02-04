/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#ifndef UNISPHERE_SOCIAL_COMPACTROUTER_H
#define UNISPHERE_SOCIAL_COMPACTROUTER_H

#include "social/social_identity.h"
#include "social/routing_table.h"

namespace UniSphere {

class LinkManager;
class NetworkSizeEstimator;
class Message;

class UNISPHERE_EXPORT CompactRouter {
public:
  /// Self-announce refresh interval
  static const int interval_announce = 10;
  /// Neighbor expiry interval
  static const int interval_neighbor_expiry = 30;
  /// Route origin descriptor expiry time
  static const int origin_expiry_time = 300;

  CompactRouter(SocialIdentity &identity, LinkManager &manager,
                NetworkSizeEstimator &sizeEstimator);

  void initialize();

  void shutdown();

  /**
   * Returns the UNISPHERE context this router belongs to.
   */
  inline Context &context() const { return m_context; }

  /**
   * Returns the reference to underlying social identity.
   */
  const SocialIdentity &identity() const { return m_identity; }

  /**
   * Returns the reference to underlying routing table.
   */
  CompactRoutingTable &routingTable() { return m_routes; }
protected:
  /**
   * Called when a message has been received on any link.
   * 
   * @param msg Link-local message that has been received
   */
  void messageReceived(const Message &msg);

  void networkSizeEstimateChanged(std::uint64_t size);

  /**
   * Announces the current node to all neighbors.
   */
  void announceOurselves(const boost::system::error_code &error);

  void requestFullRoutes();

  void peerAdded(const Contact &peer);

  void peerRemoved(const NodeIdentifier &nodeId);

  /**
   * A handler for verification of peer contacts.
   *
   * @param peer Peer contact to verify
   * @return True if verification is successful, false otherwise
   */
  bool linkVerifyPeer(const Contact &peer);

  /**
   * Called by the routing table when an entry should be exported to
   * all neighbors.
   *
   * @param entry Routing entry to export
   * @param peer Optional peer identifier to export to
   */
  void ribExportEntry(RoutingEntryPtr entry,
    const NodeIdentifier &peer = NodeIdentifier::INVALID);

  /**
   * Called by the routing table when a retraction should be sent to
   * all neighbors.
   *
   * @param entry Routing entry to retract
   */
  void ribRetractEntry(RoutingEntryPtr entry);
private:
  /// UNISPHERE context
  Context &m_context;
  /// Local node identity
  SocialIdentity &m_identity;
  /// Link manager associated with this router
  LinkManager &m_manager;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Compact routing table
  CompactRoutingTable m_routes;
  /// Timer for notifying neighbours about ourselves
  boost::asio::deadline_timer m_announceTimer;
  /// Active subscriptions to other components
  std::list<boost::signals::connection> m_subscriptions;
  /// Local sequence number
  std::uint16_t m_seqno;
};
  
}

#endif
