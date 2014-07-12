/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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

#include "core/globals.h"
#include "core/signal.h"
#include "social/routed_message.h"

#include <boost/signals2/signal.hpp>
#include <unordered_map>

namespace UniSphere {

class Context;
class SocialIdentity;
class LinkManager;
class NetworkSizeEstimator;
class Message;
class CompactRoutingTable;
class NameDatabase;
class SloppyGroupManager;
class SocialRpcChannel;
#ifdef UNISPHERE_PROFILE
class MessageTracer;
#endif

template <typename Channel>
class RpcEngine;

/**
 * Compact router is at the core of the UNISPHERE protocol. It binds all the
 * components together and routes messages.
 */
class UNISPHERE_EXPORT CompactRouter {
public:
  /// Self-announce refresh interval
  static const int interval_announce = 10;
  /// Neighbor expiry interval
  static const int interval_neighbor_expiry = 60;
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
    SloppyGroup   = 0x02,
  };

  /**
   * A structure for reporting router statistics.
   */
  struct Statistics {
    struct LinkStatistics {
      /// Number of transmitted messages
      size_t msgXmits = 0;
      /// Number of received messages
      size_t msgRcvd = 0;
    };

    /// Per-link statistics
    std::unordered_map<NodeIdentifier, LinkStatistics> links;
    /// Number of transmitted routing entries
    size_t entryXmits = 0;
    /// Number of SA-related update transmissions
    size_t saUpdateXmits = 0;
    /// Number of messages routed via a landmark node
    size_t msgsLandmarkRouted = 0;
    /// Number of messages routed via a sloppy group member
    size_t msgsSgRouted = 0;
  };

  /**
   * Class constructor.
   *
   * @param identity Social identity
   * @param manager Link manager
   * @param sizeEstimator Network size estimator
   */
  CompactRouter(SocialIdentity &identity,
                LinkManager &manager,
                NetworkSizeEstimator &sizeEstimator);

  CompactRouter(const CompactRouter&) = delete;
  CompactRouter &operator=(const CompactRouter&) = delete;

  /**
   * Initializes the router.
   */
  void initialize();

  /**
   * Shuts down the router and all components.
   */
  void shutdown();

  /**
   * Returns the UNISPHERE context this router belongs to.
   */
  Context &context() const;

  /**
   * Returns the reference to underlying social identity.
   */
  const SocialIdentity &identity() const;

  /**
   * Returns the link manager instance associated with this router.
   */
  const LinkManager &linkManager() const;

  /**
   * Returns the reference to underlying routing table.
   */
  CompactRoutingTable &routingTable();

  /**
   * Returns the reference to underlying name database.
   */
  NameDatabase &nameDb();

  /**
   * Returns the reference to the sloppy group manager.
   */
  SloppyGroupManager &sloppyGroup();

  /**
   * Returns the reference to underlying RPC engine.
   */
  RpcEngine<SocialRpcChannel> &rpcEngine();

#ifdef UNISPHERE_PROFILE
  /**
   * Returns a reference to message tracer. Only available when
   * profiling is enabled at compile-time.
   */
  MessageTracer &msgTracer();
#endif

  /**
   * Retrieves various statistics about router operation.
   */
  const Statistics &statistics() const;

  /**
   * Routes the specified message via the overlay.
   *
   * @param msg A valid message to be routed
   */
  void route(RoutedMessage &msg);

  /**
   * Generates a new message and routes it via the overlay.
   *
   * @param sourceCompId Source component identifier
   * @param destination Destination node identifier
   * @param destinationAddress Destination L-R address (if known)
   * @param destinationCompId Destination component identifier
   * @param type Message type
   * @param msg Protocol Buffers message
   * @param opts Routing options
   */
  void route(std::uint32_t sourceCompId,
             const NodeIdentifier &destination,
             const LandmarkAddress &destinationAddress,
             std::uint32_t destinationCompId,
             std::uint32_t type,
             const google::protobuf::Message &msg,
             const RoutingOptions &opts = RoutingOptions());
public:
  /// Signal for delivery of locally-bound messages
  boost::signals2::signal<void(const RoutedMessage&)> signalDeliverMessage;
  /// Signal for forwarding transit messages
  boost::signals2::signal<bool(const RoutedMessage&), AllTrueCombiner> signalForwardMessage;
private:
  UNISPHERE_DECLARE_PRIVATE(CompactRouter)
};

}

#endif
