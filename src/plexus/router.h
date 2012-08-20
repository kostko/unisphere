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

namespace UniSphere {
  
class LinkManager;

class UNISPHERE_EXPORT Router {
public:
  /// Bucket size (routing redundancy)
  static const size_t bucket_size = 20;
  /// Per-key sibling neighbourhood size (key storage redundancy)
  static const size_t sibling_neighbourhood = 16;
  
  /**
   * Constructs a router instance.
   * 
   * @param manager Link manager used for underlaying communication
   */
  Router(LinkManager &manager);
  
  //void route(destination, msg...?)
  
  // TODO periodic refresh of buckets when no communication has been received from them
  // in a while (also see "Handling churn in a DHT")
public:
  // Signals
  boost::signal<void(Message&)> signalDeliverMessage;
  boost::signal<void(Message&)> signalForwardMessage;
  // TODO signal when local siblings have changed
private:
  /// Reference to link manager for this router
  LinkManager &m_manager;
  /// The routing table
  RoutingTable m_routes;
};
  
}

#endif
