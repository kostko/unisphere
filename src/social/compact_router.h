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
#include "social/routed_message.h"
#include "social/name_database.h"
#include "social/rpc_engine.h"

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

  /**
   * Identifiers of components that can be routed to. These components
   * may differ between nodes, but system components must always be
   * implemented.
   */
  enum class Component : std::uint32_t {
    /* 0x00 - 0xFF RESERVED FOR SYSTEM PROTOCOLS */
    Null          = 0x00,
    RPC_Engine    = 0x01,
  };

  CompactRouter(SocialIdentity &identity, LinkManager &manager,
                NetworkSizeEstimator &sizeEstimator);

  CompactRouter(const CompactRouter&) = delete;
  CompactRouter &operator=(const CompactRouter&) = delete;

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
   * Returns the link manager instance associated with this router.
   */
  const LinkManager &linkManager() const { return m_manager; }

  /**
   * Returns the reference to underlying routing table.
   */
  CompactRoutingTable &routingTable() { return m_routes; }

  /**
   * Returns the reference to underlying RPC engine.
   */
  RpcEngine &rpcEngine() { return m_rpc; }

  /**
   * Routes the specified message via the overlay.
   * 
   * @param msg A valid message to be routed
   */
  void route(const RoutedMessage &msg);
  
  /**
   * Generates a new message and routes it via the overlay.
   * 
   * @param sourceCompId Source component identifier
   * @param destination Destination node identifier
   * @param destinationCompId Destination component identifier
   * @param type Message type
   * @param msg Protocol Buffers message
   * @param opts Routing options
   */
  void route(std::uint32_t sourceCompId, const NodeIdentifier &destination,
             std::uint32_t destinationCompId, std::uint32_t type,
             const google::protobuf::Message &msg,
             const RoutingOptions &opts = RoutingOptions());
public:
  /// Signal for delivery of locally-bound messages
  boost::signal<void(const RoutedMessage&)> signalDeliverMessage;
  /// Signal for forwarding transit messages
  boost::signal<void(const RoutedMessage&)> signalForwardMessage;
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

  void landmarkLearned(const NodeIdentifier &landmarkId);

  void landmarkRemoved(const NodeIdentifier &landmarkId);

  /**
   * Performs registration of core RPC methods that are required for routing.
   */
  void registerCoreRpcMethods();

  /**
   * Performs unregistration of core RPC methods that are required for routing.
   */
  void unregisterCoreRpcMethods();
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
  /// Name database
  NameDatabase m_nameDb;
  /// RPC engine
  RpcEngine m_rpc;
  /// Timer for notifying neighbours about ourselves
  boost::asio::deadline_timer m_announceTimer;
  /// Active subscriptions to other components
  std::list<boost::signals::connection> m_subscriptions;
  /// Local sequence number
  std::uint16_t m_seqno;
};
  
}

#endif
