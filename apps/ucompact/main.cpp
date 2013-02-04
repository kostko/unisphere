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
#include <fstream>
#include <string>

#include <botan/auto_rng.h>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bimap.hpp>

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
  const std::string &ip, unsigned short port)
{
  VirtualNode *node = new VirtualNode();
  node->nodeId = nodeId;
  node->identity = new SocialIdentity(nodeId);
  node->linkManager = new LinkManager(context, nodeId);
  node->linkManager->setLocalAddress(Address(ip, 0));
  node->linkManager->listen(Address(ip, port));
  node->router = new CompactRouter(*node->identity, *node->linkManager, sizeEstimator);
  return node;
}

int main(int argc, char **argv)
{
  // Open the social topology datafile
  std::ifstream socialTopologyFile("../data/social_topology.dat");
  if (!socialTopologyFile) {
    std::cerr << "ERROR: Missing ../data/social_topology.dat file!" << std::endl;
    return EXIT_FAILURE;
  }

  // Read the number of nodes
  std::string line;
  std::getline(socialTopologyFile, line);
  int nodeCount = boost::lexical_cast<int>(line);

  LibraryInitializer init;
  Context ctx;
  OracleNetworkSizeEstimator sizeEstimator(nodeCount);

  typedef boost::bimap<std::string, NodeIdentifier> NodeNameMap;
  typedef std::unordered_map<NodeIdentifier, VirtualNode*> VirtualNodeMap;
  NodeNameMap names;
  VirtualNodeMap nodes;
  unsigned short port = 8473;

  // Parse social topology
  for (;std::getline(socialTopologyFile, line);) {
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(",");
    tokenizer tok(line, sep);
    tokenizer::iterator i = tok.begin();

    // Try to resolve names - if none are assigned, generate new ones
    auto getIdFromName = [&](const std::string &name) -> NodeIdentifier {
      NodeIdentifier nodeId;
      auto idi = names.left.find(name);
      if (idi == names.left.end()) {
        nodeId = getRandomNodeId();
        names.insert(NodeNameMap::value_type(name, nodeId));
      } else {
        nodeId = (*idi).second;
      }

      return nodeId;
    };
    
    NodeIdentifier nodeA = getIdFromName(*i++);
    NodeIdentifier nodeB = getIdFromName(*i++);

    if (nodeA == nodeB) {
      std::cerr << "ERROR: Invalid link to self (node=" << nodeA.as(NodeIdentifier::Format::Hex) << ")" << std::endl;
      return EXIT_FAILURE;
    }

    auto getNodeFromId = [&](const NodeIdentifier &nodeId) -> VirtualNode* {
      VirtualNode *node;
      VirtualNodeMap::iterator ni = nodes.find(nodeId);
      if (ni == nodes.end()) {
        node = createNode(ctx, sizeEstimator, nodeId, "127.42.0.1", port++);
        nodes[nodeId] = node;
      } else {
        node = (*ni).second;
      }

      return node;
    };

    VirtualNode *a = getNodeFromId(nodeA);
    VirtualNode *b = getNodeFromId(nodeB);

    a->identity->addPeer(b->linkManager->getLocalContact());
  }

  // Initialize all nodes
  for (VirtualNodeMap::iterator i = nodes.begin(); i != nodes.end(); ++i) {
    (*i).second->router->initialize();
  }

  // Shutdown the first node after 40 seconds
  /*ctx.schedule(40, [&]() {
    nodes.begin()->second->router->shutdown();
  });*/

  // Schedule routing table dump after 80 seconds
  ctx.schedule(80, [&]() {
    for (VirtualNodeMap::iterator i = nodes.begin(); i != nodes.end(); ++i) {
      std::cout << "---- ROUTING TABLE FOR: " << (*i).first.as(NodeIdentifier::Format::Hex) << " (" << names.right.at((*i).first) << ") ----" << std::endl;
      (*i).second->router->routingTable().dump(std::cout, [&](const NodeIdentifier &n) -> std::string { return names.right.at(n); });
    }
  });
  
  // Run the context
  ctx.run(1);
  return 0;
}
