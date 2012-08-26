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
#ifndef UNISPHERE_PLEXUS_ROUTER_H
#define UNISPHERE_PLEXUS_ROUTER_H

#include "core/context.h"
#include "plexus/routing_table.h"
#include "plexus/routed_message.h"
#include "plexus/rpc_engine.h"

namespace UniSphere {
  
class LinkManager;
class Bootstrap;

/**
 * Router is used to route messages over the overlay network and ensure that 
 * routing tables are up to date.
 */
class UNISPHERE_EXPORT Router {
public:
  /// Bucket size (routing redundancy)
  static const size_t bucket_size = 20;
  /// Per-key sibling neighbourhood size (key storage redundancy)
  static const size_t sibling_neighbourhood = 16;
  
  /**
   * Identifiers of components that can be routed to. These components
   * may differ between nodes, but system components must always be
   * implemented.
   */
  enum class Component : std::uint32_t {
    /* 0x00 - 0xFF RESERVED FOR SYSTEM PROTOCOLS */
    RPC_Engine    = 0x01,
  };
  
  /**
   * Constructs a router instance.
   * 
   * @param manager Link manager used for underlaying communication
   * @param bootstrap Bootstrap mechanism
   */
  Router(LinkManager &manager, Bootstrap &bootstrap);
  
  /**
   * Returns the link manager instance associated with this router.
   */
  inline LinkManager &linkManager() const { return m_manager; }
  
  /**
   * Joins the overlay network by using the specified bootstrap mechanism.
   */
  void join();
  
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
   * @param key Destination key
   * @param destinationCompId Destination component identifier
   * @param type Message type
   * @param msg Protocol Buffers message
   */
  void route(std::uint32_t sourceCompId, const NodeIdentifier &key,
             std::uint32_t destinationCompId, std::uint32_t type,
             const google::protobuf::Message &msg);
  
  // TODO periodic refresh of buckets when no communication has been received from them
  // in a while (also see "Handling churn in a DHT")
protected:
  /**
   * A method that initializes each link by connecting to its signals that
   * are required to manage the routing table.
   * 
   * @param link Link to initialize
   */
  void initializeLink(Link &link);
  
  /**
   * Called when a message has been received on any link.
   * 
   * @param msg Link-local message that has been received
   */
  void linkMessageReceived(const Message &msg);
  
  /**
   * Called when a link has been established.
   * 
   * @param link Link that has been established
   */
  void linkEstablished(LinkPtr link);
  
  /**
   * Called when a link has been lost (disconnected).
   * 
   * @param link Link that has been lost
   */
  void linkLost(LinkPtr link);
public:
  /// Signal for delivery of locally-bound messages
  boost::signal<void(const RoutedMessage&)> signalDeliverMessage;
  /// Signal for forwarding transit messages
  boost::signal<void(const RoutedMessage&)> signalForwardMessage;
  // TODO signal when local siblings have changed
private:
  /// Reference to link manager for this router
  LinkManager &m_manager;
  /// The bootstrap mechanism used
  Bootstrap &m_bootstrap;
  /// The routing table
  RoutingTable m_routes;
  /// The RPC engine
  RpcEngine m_rpc;
};
  
}

#endif
