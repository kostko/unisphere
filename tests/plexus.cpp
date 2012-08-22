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
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "plexus/routing_table.h"

#include <random>

using namespace UniSphere;

TEST_CASE("plexus/routing_table", "verify that routing table operations work")
{
  // Local node identifier
  NodeIdentifier localId("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
  // Routing table parameters
  const size_t routingRedundancyK = 20;
  const size_t replicaRedundancyS = routingRedundancyK / 2;
  // The routing table
  RoutingTable rt(localId, routingRedundancyK, replicaRedundancyS);
  // The random generator
  std::mt19937 rng;
  rng.seed(42);
  
  SECTION("simple", "test simple entry insertion and lookup")
  {
    NodeIdentifier node("3c972273f4d3db642d4585715324e66cbbce024b", NodeIdentifier::Format::Hex);
    // Insertion should modify the routing table
    REQUIRE(rt.add(node) == true);
    // Insertion into an empty routing table should insert into the sibling table
    REQUIRE(rt.siblingCount() == 1);
    // No additional buckets should have been allocated (1 is by default)
    REQUIRE(rt.bucketCount() == 1);
    REQUIRE(rt.peerCount() == 0);
    
    // Should be sibling for the local node
    REQUIRE(rt.isSiblingFor(node, localId) == true);
    
    // Ensure that rejoin signal is called when the routing table gets empty
    bool rejoinCalled = false;
    rt.signalRejoin.connect([&] () { rejoinCalled = true; });
    REQUIRE(rt.remove(node) == true);
    REQUIRE(rejoinCalled == true);
  }
  
  // Node identifiers that share half the bits with the local node identifier
  std::list<NodeIdentifier> siblings;
  // For choosing a random prefix size between 8 and 12 bytes
  std::uniform_int_distribution<int> prefixSize(8, 12);
  // For choosing a random suffix
  std::uniform_int_distribution<int> suffixByte(0, 255);
  for (int i = 0; i < replicaRedundancyS * 2; i++) {
    // Generate a random sibling identifier
    std::string id = localId.as(NodeIdentifier::Format::Raw).substr(0, prefixSize(rng));
    // All remaining bytes are chosen at random
    while (id.size() < NodeIdentifier::length) {
      id += static_cast<char>(suffixByte(rng));
    }
    
    NodeIdentifier siblingId(id, NodeIdentifier::Format::Raw);
    REQUIRE(siblingId.isValid());
    siblings.push_back(siblingId);
  }
  
  SECTION("siblings", "test sibling insertion")
  {
    for (const NodeIdentifier &siblingId : siblings) {
      // Insertions must modify the routing table
      REQUIRE(rt.add(siblingId) == true);
    }
    
    // Ensure that all entries have appeared in the sibling table
    REQUIRE(rt.siblingCount() == siblings.size());
    // No additional buckets should have been allocated
    REQUIRE(rt.bucketCount() == 1);
    REQUIRE(rt.peerCount() == 0);
    
    // Insert additional entries to cause an overflow of the sibling table; enough
    // to spill into a single (first) k-bucket
    std::list<NodeIdentifier> additionalPeers;
    for (int i = 0; i < replicaRedundancyS * 4; i++) {
      std::string id;
      while (id.size() < NodeIdentifier::length) {
        id += static_cast<char>(suffixByte(rng));
      }
      
      NodeIdentifier nodeId(id, NodeIdentifier::Format::Raw);
      additionalPeers.push_back(nodeId);
      REQUIRE(nodeId.isValid());
      REQUIRE(rt.add(nodeId) == true);
    }
    
    // Sibling count should now be the maximum and should stay this way unless
    // nodes start being removed
    REQUIRE(rt.siblingCount() == replicaRedundancyS * 5);
    // Only the first bucket should be populated
    REQUIRE(rt.bucketCount() == 1);
    // Check that the number of spilled peers is correct
    REQUIRE(rt.peerCount() == replicaRedundancyS);
    
    // Ensure that all siblings are contained in the sibling table
    DistanceOrderedTable siblingTable = rt.lookup(localId, replicaRedundancyS * 5);
    REQUIRE(siblingTable.table().size() == replicaRedundancyS * 5);
    for (const NodeIdentifier &siblingId : siblings) {
      REQUIRE(siblingTable.table().get<NodeIdTag>().find(siblingId) != siblingTable.table().end());
    }
    
    SECTION("removal/buckets", "test that removal of entries from buckets works")
    {
      // Monitor status of the rejoin signal that must not be called
      bool rejoinCalled = false;
      rt.signalRejoin.connect([&] () { rejoinCalled = true; });
      
      // Remove peer entries that are contained in buckets (= those that are not in the
      // sibling table)
      size_t bucketPeers = rt.peerCount();
      size_t siblingPeers = rt.siblingCount();
      for (const NodeIdentifier &peerId : additionalPeers) {
        if (siblingTable.table().get<NodeIdTag>().find(peerId) == siblingTable.table().end()) {
          REQUIRE(rt.remove(peerId) == true);
          REQUIRE(rejoinCalled == false);
          bucketPeers--;
          REQUIRE(rt.peerCount() == bucketPeers);
        }
      }
      
      // At the end we must have zero peers in buckets (all have been removed) but the sibling
      // count must remain unchanged
      REQUIRE(rt.peerCount() == 0);
      REQUIRE(rt.siblingCount() == siblingPeers);
    }
    
    SECTION("removal/siblings", "test that removal of siblings works")
    {
      // We remove a sibling and check that the sibling table gets refilled with an entry
      // from the peer k-buckets
      size_t oldPeerCount = rt.peerCount();
      size_t oldSiblingCount = rt.siblingCount();
      
      REQUIRE(rt.remove(siblings.back()) == true);
      REQUIRE(rt.siblingCount() == oldSiblingCount);
      REQUIRE(rt.peerCount() == oldPeerCount - 1);
    }
    
    SECTION("random", "fill the routing table")
    {
      // Now insert lots of random entries and check that the routing table behaves
      // as it should
      for (int i = 0; i < routingRedundancyK * 20; i++) {
        std::string id;
        while (id.size() < NodeIdentifier::length) {
          id += static_cast<char>(suffixByte(rng));
        }
        
        NodeIdentifier nodeId(id, NodeIdentifier::Format::Raw);
        REQUIRE(nodeId.isValid());
        rt.add(nodeId);
      }
      
      // TODO Further tests
    }
  }
}

TEST_CASE("plexus/router", "verify that the router works")
{
}
