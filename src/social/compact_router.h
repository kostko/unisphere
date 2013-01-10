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
  CompactRouter(const SocialIdentity &identity, LinkManager &manager,
                NetworkSizeEstimator &sizeEstimator);

  void initialize();
protected:
  /**
   * Called when a message has been received on any link.
   * 
   * @param msg Link-local message that has been received
   */
  void messageReceived(const Message &msg);

  void networkSizeEstimateChanged(std::uint64_t size);

  void announceOurselves(const boost::system::error_code &error);

  /**
   * Called by the routing table when an entry should be exported to
   * all neighbors.
   *
   * @param entry Routing entry to export
   */
  void ribExportEntry(const RoutingEntry &entry);

  /**
   * Called by the routing table when a retraction should be sent to
   * all neighbors.
   *
   * @param entry Routing entry to retract
   */
  void ribRetractEntry(const RoutingEntry &entry);
private:
  /// Local node identity
  SocialIdentity m_identity;
  /// Link manager associated with this router
  LinkManager &m_manager;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Compact routing table
  CompactRoutingTable m_routes;
  /// Timer for notifying neighbours about ourselves
  boost::asio::deadline_timer m_announceTimer;
};
  
}

#endif
