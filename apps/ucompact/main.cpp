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
#include "core/context.h"
#include "interplex/link_manager.h"
#include "social/compact_router.h"
#include "social/size_estimator.h"

#include <iostream>
#include <botan/auto_rng.h>
#include <boost/format.hpp>

using namespace UniSphere;

struct VirtualNode {
  NodeIdentifier nodeId;
  SocialIdentity *identity;
  LinkManager *linkManager;
  CompactRouter *router;
};

NodeIdentifier getRandomNodeId()
{
  Botan::AutoSeeded_RNG rng;
  char nodeId[NodeIdentifier::length];
  rng.randomize((Botan::byte*) &nodeId, sizeof(nodeId));
  return NodeIdentifier(std::string(nodeId, sizeof(nodeId)), NodeIdentifier::Format::Raw);
}

VirtualNode *createNode(Context &context, NetworkSizeEstimator &sizeEstimator, const NodeIdentifier &nodeId,
  const std::string &ip, unsigned short port, const Contact &bootstrap = Contact())
{
  VirtualNode *node = new VirtualNode();
  node->nodeId = nodeId;
  node->identity = new SocialIdentity(nodeId);
  node->identity->addPeer(bootstrap);
  node->linkManager = new LinkManager(context, nodeId);
  node->linkManager->setLocalAddress(Address(ip, 0));
  node->linkManager->listen(Address(ip, port));
  node->router = new CompactRouter(*node->identity, *node->linkManager, sizeEstimator);
  node->router->initialize();
  return node;
}

int main(int argc, char **argv)
{
  LibraryInitializer init;
  Context ctx;
  OracleNetworkSizeEstimator sizeEstimator(2);

  // TODO: Build a graph of nodes based on some predefined topology

  // Create the bootstrap node
  VirtualNode *bootstrap = createNode(ctx, sizeEstimator, getRandomNodeId(), "127.42.0.1", 8472);
  
  std::unordered_map<NodeIdentifier, VirtualNode*> nodes;
  unsigned short port = 8473;
  for (int i = 0; i < sizeEstimator.getNetworkSize() - 1; i++) {
    VirtualNode *node = createNode(ctx, sizeEstimator, getRandomNodeId(), "127.42.0.1", port++, bootstrap->linkManager->getLocalContact());
    bootstrap->identity->addPeer(node->linkManager->getLocalContact());
    nodes[node->nodeId] = node;
  }
  
  // Run the context
  ctx.run(1);
  return 0;
}
