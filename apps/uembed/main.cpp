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
#include "src/plexus/core_methods.pb.h"
#include "measure/aggregate.h"

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
  VirtualNode *node = new VirtualNode();
  node->nodeId = nodeId;
  node->linkManager = new LinkManager(context, nodeId);
  node->linkManager->setLocalAddress(Address(ip, 0));
  node->linkManager->listen(Address(ip, port));
  node->bootstrap = new DelayedBootstrap();
  if (!bootstrap.isNull())
    node->bootstrap->addContact(bootstrap);
  node->router = new Router(*node->linkManager, *node->bootstrap);
  return node;
}

int main(int argc, char **argv)
{
  LibraryInitializer init;
  Context ctx;

  // Create the bootstrap node
  VirtualNode *bootstrap = createNode(ctx, getRandomNodeId(), "127.42.0.1", 8472);
  
  std::unordered_map<NodeIdentifier, VirtualNode*> nodes;
  unsigned short port = 8473;
  for (int i = 0; i < 50; i++) {
    VirtualNode *node = createNode(ctx, getRandomNodeId(), "127.42.0.1", port++, bootstrap->linkManager->getLocalContact());
    nodes[node->nodeId] = node;
  }
  
  // Bootstrap the network
  bootstrap->router->create();
  
  // Join peers at a specific rate
  int delay = 1;
  std::unordered_map<NodeIdentifier, VirtualNode*>::iterator peer = nodes.begin();
  boost::asio::deadline_timer joinTimer(ctx.service());
  std::function<void()> joinNode = [&]() {
    if (peer == nodes.end())
      return;
    
    (*peer).second->router->join();
    ++peer;
    
    joinTimer.expires_from_now(boost::posix_time::seconds(delay));
    joinTimer.async_wait(boost::bind(joinNode));
  };
  
  joinTimer.expires_from_now(boost::posix_time::seconds(delay));
  joinTimer.async_wait(boost::bind(joinNode));
  
  // Collect some statistics after 3 minutes
  ctx.schedule(180, [&]() {
    AggregateMeasure agg;
    for (std::pair<NodeIdentifier, VirtualNode*> p : nodes) {
      agg.add(p.second->linkManager->getMeasure());
    }
    
    for (std::pair<std::string, AggregateMetric> p : agg.metrics()) {
      std::cout << "Metric: " << p.first << std::endl;
      std::cout << "Mean: " << p.second.mean() << " Min: " << p.second.min() << " Max: " << p.second.max() << std::endl;
      std::cout << "StdDev: " << p.second.std() << std::endl;
      std::cout << std::endl;
    }
  });
  
  // After 3 minutes, check if RPC calls between nodes work
  /*
  ctx.schedule(180, [&]() {
    for (std::pair<NodeIdentifier, VirtualNode*> p : nodes) {
      VirtualNode *node = p.second;
      Protocol::PingRequest ping;
      ping.set_timestamp(0);
      bootstrap->router->rpcEngine().call<Protocol::PingRequest, Protocol::PingResponse>(node->nodeId, "Core.Ping", ping,
        // Success handler
        [](const Protocol::PingResponse &response, const RoutedMessage&) {
          std::cout << "success!" << std::endl;
        },
        // Error handler
        [](RpcErrorCode, const std::string&) {
          std::cout << "failure failure failure" << std::endl;
        }
      );
    }
  });
  
  // After 3 minutes and 30 seconds, bootstrap node leaves the overlay
  ctx.schedule(210, [&]() {
    bootstrap->router->leave();
  });*/
  
  // Run the context
  ctx.run(2);
  return 0;
}
