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
#ifndef UNISPHERE_TESTBED_NODES_H
#define UNISPHERE_TESTBED_NODES_H

#include <unordered_map>
#include <boost/bimap.hpp>

#include "identity/node_identifier.h"

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
  VirtualNode(Context &context,
              NetworkSizeEstimator &sizeEstimator,
              const NodeIdentifier &nodeId,
              const std::string &ip,
              unsigned short port);

  ~VirtualNode();

  VirtualNode(const VirtualNode&) = delete;
  VirtualNode &operator=(const VirtualNode&) = delete;

  void initialize();

  void shutdown();
public:
  /// Unique node identifier
  NodeIdentifier nodeId;
  /// Node's social identity (peers)
  SocialIdentity *identity;
  /// Transport link manager for this node
  LinkManager *linkManager;
  /// Router for this node
  CompactRouter *router;
};

/// Mapping between original node names and generated identifiers
typedef boost::bimap<std::string, NodeIdentifier> NodeNameMap;
/// Virtual nodes running in our testbed
typedef std::unordered_map<NodeIdentifier, VirtualNode*> VirtualNodeMap;

}

}

#endif
