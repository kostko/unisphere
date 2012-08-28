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
#include "core/context.h"
#include "interplex/link_manager.h"
#include "plexus/bootstrap.h"
#include "plexus/router.h"

#include <iostream>
#include <botan/auto_rng.h>
#include <boost/format.hpp>

using namespace UniSphere;

struct VirtualNode {
  NodeIdentifier nodeId;
  LinkManager *linkManager;
  DelayedBootstrap *bootstrap;
  Router *router;
};

NodeIdentifier getRandomNodeId()
{
  Botan::AutoSeeded_RNG rng;
  char nodeId[NodeIdentifier::length];
  rng.randomize((Botan::byte*) &nodeId, sizeof(nodeId));
  return NodeIdentifier(std::string(nodeId, sizeof(nodeId)), NodeIdentifier::Format::Raw);
}

VirtualNode *createNode(Context &context, const NodeIdentifier &nodeId, const std::string &ip, unsigned short port,
  const Contact &bootstrap = Contact())
{
  std::cout << "creating node with id: " << nodeId.as(NodeIdentifier::Format::Hex) << std::endl;
  VirtualNode *node = new VirtualNode();
  node->nodeId = nodeId;
  node->linkManager = new LinkManager(context, nodeId);
  node->linkManager->setLocalAddress(Address(ip, 0));
  node->linkManager->listen(Address(ip, port));
  node->bootstrap = new DelayedBootstrap();
  if (!bootstrap.isNull())
    node->bootstrap->addContact(bootstrap);
  node->router = new Router(*node->linkManager, *node->bootstrap);
}

int main(int argc, char **argv)
{
  LibraryInitializer init;
  Context ctx;
  
  VirtualNode *first = createNode(ctx, getRandomNodeId(), "127.42.0.1", 8472);
  VirtualNode *second = createNode(ctx, getRandomNodeId(), "127.42.0.2", 8472, first->linkManager->getLocalContact());
  second->router->join();
  
  // Run the context
  ctx.run();
  return 0;
}
