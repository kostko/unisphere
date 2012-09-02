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

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

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
  static const size_t bucket_size = 16;
  /// Per-key sibling neighbourhood size (key storage redundancy)
  static const size_t sibling_neighbourhood = 8;
  /// Pending contacts establishment period (in seconds)
  static const int pending_contacts_period = 2;
  /// Maximum number of pending contacts
  static const size_t pending_contacts_size = 128;
  
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
   * Possible states of the overlay router.
   */
  enum class State {
    Init,
    Bootstrap,
    Joined,
    Leaving
  };
  
  /**
   * Constructs a router instance.
   * 
   * @param manager Link manager used for underlaying communication
   * @param bootstrap Bootstrap mechanism
   */
  Router(LinkManager &manager, Bootstrap &bootstrap);
  
  Router(const Router&) = delete;
  Router &operator=(const Router&) = delete;
  
  /**
   * Returns the link manager instance associated with this router.
   */
  inline LinkManager &linkManager() const { return m_manager; }
  
  /**
   * Returns the RPC engine instance associated with this router.
   */
  inline RpcEngine &rpcEngine() { return m_rpc; }
  
  /**
   * Returns the current router state.
   */
  inline State state() { return m_state; }
  
  /**
   * Joins the overlay network by using the specified bootstrap mechanism.
   */
  void join();
  
  /**
   * Creates the overlay network by being the only node in it.
   */
  void create();
  
  /**
   * Leaves the overlay network.
   */
  void leave();
  
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
   * @param opts Routing options
   */
  void route(std::uint32_t sourceCompId, const NodeIdentifier &key,
             std::uint32_t destinationCompId, std::uint32_t type,
             const google::protobuf::Message &msg,
             const RoutingOptions &opts = RoutingOptions());
  
  // TODO periodic refresh of buckets when no communication has been received from them
  // in a while (also see "Handling churn in a DHT")
protected:
  /**
   * Called when a message has been received on any link.
   * 
   * @param msg Link-local message that has been received
   */
  void messageReceived(const Message &msg);
  
  /**
   * Pings a contact for addition into the routing table. The ping message is
   * delivered directly and if the contact replies it is added to the routing
   * table.
   * 
   * @param contact Contact to ping
   */
  void pingContact(const Contact &contact);
  
  /**
   * Performs registration of core RPC methods that are required for routing.
   */
  void registerCoreRpcMethods();
public:
  /// Signal for delivery of locally-bound messages
  boost::signal<void(const RoutedMessage&)> signalDeliverMessage;
  /// Signal for forwarding transit messages
  boost::signal<void(const RoutedMessage&)> signalForwardMessage;
  /// Signal when the overlay becomes ready
  boost::signal<void()> signalJoined;
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
  /// Mutex protecting the router
  std::recursive_mutex m_mutex;
  /// Router state
  State m_state;
};
  
}

#endif
