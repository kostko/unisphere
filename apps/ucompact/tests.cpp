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
#include "testbed/test_bed.h"
#include "social/compact_router.h"
#include "social/rpc_engine.h"

#include "src/social/core_methods.pb.h"

#include <atomic>
#include <boost/range/adaptors.hpp>

using namespace UniSphere;

namespace Tests {

class DumpNodeState : public TestBed::TestCase
{
protected:
  /**
   * Simply dump the routing state for all nodes.
   */
  void start()
  {
    auto resolveNodeName = [&](const NodeIdentifier &n) -> std::string { return names().right.at(n); };
    std::unordered_set<NodeIdentifier> authRecords;

    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      report() << "---- ROUTING STATE FOR: " << node->nodeId.hex() << " (" << names().right.at(node->nodeId) << ") ----" << std::endl;
      node->router->routingTable().dump(report(), resolveNodeName);
      node->router->nameDb().dump(report(), resolveNodeName);
      node->router->sloppyGroup().dump(report(), resolveNodeName);

      for (NameRecordPtr record : node->router->nameDb().names()) {
        if (record->type == NameRecord::Type::Authority)
          authRecords.insert(record->nodeId);
      }
    }

    report() << "---- GLOBAL AUTHORITATIVE NAME RECORDS (" << authRecords.size() << ") ----" << std::endl;
    for (NodeIdentifier nodeId : authRecords)
      report() << "  " << nodeId.hex() << " (" << names().right.at(nodeId) << ")" << std::endl;

    // Require that all node records are distributed around
    // TODO: This should be moved to a separate test
    require(authRecords.size() == nodes().size());

    report() << "---- SLOPPY GROUP TOPOLOGY ----" << std::endl;
    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      node->router->sloppyGroup().dumpTopology(report(), resolveNodeName);
    }

    finish();
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpNodeState, "state/dump_all")

class AllPairs : public TestBed::TestCase
{
protected:
  /// Number of nodes at test start
  unsigned long numNodes;
  /// Number of expected responses
  unsigned long expected;
  /// Number of received responses
  std::atomic<unsigned long> received;
  /// Number of failures
  std::atomic<unsigned long> failures;

  /**
   * Test if routing works for all pairs of nodes.
   */
  void start()
  {
    // Determine the number of nodes at test start
    numNodes = nodes().size();
    // Determine the number of expected responses
    expected = numNodes * numNodes;
    // Initialize the number of received responses
    received = 0;
    // Initialize the number of failures
    failures = 0;

    for (TestBed::VirtualNode *a : nodes() | boost::adaptors::map_values) {
      for (TestBed::VirtualNode *b : nodes() | boost::adaptors::map_values) {
        RpcEngine &rpc = a->router->rpcEngine();

        // Transmit a ping request to each node and wait for a response
        Protocol::PingRequest request;
        request.set_timestamp(1);
        rpc.call<Protocol::PingRequest, Protocol::PingResponse>(b->nodeId, "Core.Ping", request,
          [this](const Protocol::PingResponse &rsp, const RoutedMessage&) {
            received++;
            checkDone();
          },
          [this, a, b](RpcErrorCode, const std::string &msg) {
            failures++;
            report() << Logger::Level::Error << "Pair = (" << a->name << ", " << b->name << ") RPC call failure: " << msg << std::endl;
            checkDone();
          }
        );
      }
    }
  }

  /**
   * Checks if the test has been completed.
   */
  bool checkDone()
  {
    if ((received + failures) == expected)
      evaluate();
  }

  /**
   * Evaluate test results.
   */
  void evaluate()
  {
    // Test summary
    report() << "All nodes = " << numNodes << std::endl;
    report() << "Received responses = " << received << std::endl;
    report() << "Failures = " << failures << std::endl;

    // Requirements for passing the test
    require(received == expected);

    // Finish this test
    finish();
  }
};

UNISPHERE_REGISTER_TEST_CASE(AllPairs, "routing/all_pairs")

class CountState : public TestBed::TestCase
{
protected:
  /**
   * Count the amount of state all nodes are using.
   */
  void start()
  {
    unsigned long stateAllNodes = 0;
    for (TestBed::VirtualNode *node : nodes() | boost::adaptors::map_values) {
      // Routing table state
      unsigned long stateRoutingTable = node->router->routingTable().size();
      // Name database state
      unsigned long stateNameDb = node->router->nameDb().size();

      stateAllNodes += stateRoutingTable + stateNameDb;
    }

    report() << "Global state = " << stateAllNodes << std::endl;
  }
};

UNISPHERE_REGISTER_TEST_CASE(CountState, "state/count")

}
