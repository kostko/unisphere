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
#ifndef UNISPHERE_TESTBED_NODES_H
#define UNISPHERE_TESTBED_NODES_H

#include <unordered_map>
#include <boost/bimap.hpp>

#include "identity/node_identifier.h"
#include "identity/peer_key.h"
#include "interplex/contact.h"

namespace UniSphere {

class Context;
class NetworkSizeEstimator;
class SocialIdentity;
class LinkManager;
class CompactRouter;

namespace TestBed {

/**
 * A virtual node with all components needed to run it.
 */
class UNISPHERE_EXPORT VirtualNode {
public:
  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   * @param sizeEstimator Network size estimator
   * @param name Virtual node name from source topology file
   * @param contact Virtual contact
   * @param key Private peer key
   */
  VirtualNode(Context &context,
              NetworkSizeEstimator &sizeEstimator,
              const std::string &name,
              const Contact &contact,
              const PrivatePeerKey &key);

  /**
   * Class destructor.
   */
  ~VirtualNode();

  VirtualNode(const VirtualNode&) = delete;
  VirtualNode &operator=(const VirtualNode&) = delete;

  /**
   * Initializes the virtual node.
   */
  void initialize();

  /**
   * Shuts down the virtual node.
   */
  void shutdown();
public:
  /// Unique node name (from source topology file)
  std::string name;
  /// Unique node identifier
  NodeIdentifier nodeId;
  /// Node's social identity (peers)
  SocialIdentity *identity;
  /// Transport link manager for this node
  LinkManager *linkManager;
  /// Router for this node
  CompactRouter *router;
};

UNISPHERE_SHARED_POINTER(VirtualNode)

/// Virtual nodes running in our testbed
using VirtualNodeMap = std::unordered_map<NodeIdentifier, VirtualNodePtr>;

}

}

#endif
