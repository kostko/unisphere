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
#include "testbed/test_case.h"
#include "testbed/scenario.h"
#include "core/context.h"
#include "social/size_estimator.h"
#include "social/social_identity.h"
#include "social/compact_router.h"
#include "interplex/link_manager.h"

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>

namespace UniSphere {

namespace TestBed {

class TestBedPrivate {
public:
  TestBedPrivate();

  void runTest(const std::string &test);

  void runAndReschedule(int time, const std::string &test);

  void loadTopology(const std::string &filename);

  NodeIdentifier getRandomNodeId();
public:
  /// Global instance of the testbed
  static TestBed *m_self;
  /// Mutex to protect the data structure
  std::recursive_mutex m_mutex;
  /// Library initializer
  LibraryInitializer m_init;
  /// Chosen IP address for physical network
  std::string m_phyIp;
  /// Chosen starting port for physical network
  unsigned short m_phyStartPort;
  /// Framework context
  Context m_context;
  /// Size estimator
  OracleNetworkSizeEstimator *m_sizeEstimator;
  /// Virtual nodes
  VirtualNodeMap m_nodes;
  /// Name mapping for virtual nodes
  NodeNameMap m_names;
  /// Registered test case factories
  std::map<std::string, TestCaseFactoryPtr> m_testCases;
  /// Running test cases
  std::set<TestCasePtr> m_runningCases;
  /// Running scenarios
  std::set<ScenarioPtr> m_scenarios;
  /// Snapshot handler
  std::function<void()> m_snapshotHandler;
  /// Simulation start time
  boost::posix_time::ptime m_timeStart;
};

/// Global instance of the testbed
TestBed *TestBedPrivate::m_self = nullptr;

TestBedPrivate::TestBedPrivate()
  : m_phyIp(""),
    m_phyStartPort(0),
    m_sizeEstimator(nullptr)
{
}

void TestBedPrivate::runTest(const std::string &test)
{
  TestCaseFactoryPtr factory;
  TestCasePtr ptest;
  {
    RecursiveUniqueLock lock(m_mutex);
    factory = m_testCases.at(test);
    ptest = factory->create();
    m_runningCases.insert(ptest);
  }

  ptest->initialize(test, &m_nodes, &m_names);
  ptest->run();
}

void TestBedPrivate::runAndReschedule(int time, const std::string &test)
{
  runTest(test);
  m_context.schedule(time, boost::bind(&TestBedPrivate::runAndReschedule, this, time, test));
}

void TestBedPrivate::loadTopology(const std::string &filename)
{
  // TODO: Raise exceptions on failures
  if (m_phyIp.empty() || !m_phyStartPort)
    return;

  // Open the social topology datafile
  std::ifstream socialTopologyFile(filename);
  if (!socialTopologyFile) {
    // TODO: Raise exceptions on failures
    std::cerr << "ERROR: Missing " << filename << " topology file!" << std::endl;
    return;
  }

  // Parse GraphML input file for social topology
  typedef boost::adjacency_list<
    boost::listS,
    boost::listS,
    boost::undirectedS,
    boost::property<boost::vertex_name_t, std::string>,
    boost::property<boost::edge_weight_t, double>
  > Topology;

  Topology topology(0);
  boost::dynamic_properties properties;
  properties.property("label", boost::get(boost::vertex_name, topology));
  properties.property("weight", boost::get(boost::edge_weight, topology));

  try {
    boost::read_graphml(socialTopologyFile, topology, properties);
  } catch (boost::graph_exception &e) {
    std::cerr << "ERROR: Failed to parse GraphML topology!" << std::endl;
    return;
  }

  unsigned short port = m_phyStartPort;

  auto getIdFromName = [&](const std::string &name) -> NodeIdentifier {
    NodeIdentifier nodeId;
    auto idi = m_names.left.find(name);
    if (idi == m_names.left.end()) {
      nodeId = getRandomNodeId();
      m_names.insert(NodeNameMap::value_type(name, nodeId));
    } else {
      nodeId = idi->second;
    }

    return nodeId;
  };

  auto getNodeFromId = [&](const std::string &name, const NodeIdentifier &nodeId) -> VirtualNode* {
    VirtualNode *node;
    VirtualNodeMap::iterator ni = m_nodes.find(nodeId);
    if (ni == m_nodes.end()) {
      node = new VirtualNode(m_context, *m_sizeEstimator, name, nodeId, m_phyIp, port++);
      m_nodes[nodeId] = node;
    } else {
      node = ni->second;
    }

    return node;
  };

  // Create the virtual node topology
  m_sizeEstimator = new OracleNetworkSizeEstimator(boost::num_vertices(topology));
  for (auto vp = boost::vertices(topology); vp.first != vp.second; ++vp.first) {
    std::string nameA = boost::get(boost::vertex_name, topology, *vp.first);
    NodeIdentifier nodeA = getIdFromName(nameA);
    VirtualNode *a = getNodeFromId(nameA, nodeA);

    for (auto np = boost::adjacent_vertices(*vp.first, topology); np.first != np.second; ++np.first) {
      std::string nameB = boost::get(boost::vertex_name, topology, *np.first);
      NodeIdentifier nodeB = getIdFromName(nameB);
      VirtualNode *b = getNodeFromId(nameB, nodeB);

      // Avoid self-loops
      if (a == b)
        continue;

      // Add peers based on the social graph topology
      a->identity->addPeer(b->linkManager->getLocalContact());
    }
  }
}

NodeIdentifier TestBedPrivate::getRandomNodeId()
{
  Botan::AutoSeeded_RNG rng;
  char nodeId[NodeIdentifier::length];
  rng.randomize((Botan::byte*) &nodeId, sizeof(nodeId));
  return NodeIdentifier(std::string(nodeId, sizeof(nodeId)), NodeIdentifier::Format::Raw);
}


TestBed::TestBed()
  : d(new TestBedPrivate)
{
}

TestBed &TestBed::getGlobalTestbed()
{
  if (!TestBedPrivate::m_self)
    TestBedPrivate::m_self = new TestBed();

  return *TestBedPrivate::m_self;
}

Context &TestBed::getContext()
{
  return d->m_context;
}

void TestBed::setupPhyNetwork(const std::string &ip, unsigned short port)
{
  d->m_phyIp = ip;
  d->m_phyStartPort = port;
}

void TestBed::run()
{
  // Initialize all nodes
  for (VirtualNode *node : d->m_nodes | boost::adaptors::map_values)
    node->initialize();

  d->m_timeStart = boost::posix_time::microsec_clock::universal_time();

  // Run the context
  for (;;) {
    d->m_context.run(8);

    if (d->m_snapshotHandler) {
      // When a snapshot handler is defined, we should invoke it and restart
      d->m_snapshotHandler();
      d->m_snapshotHandler = nullptr;
      continue;
    }

    break;
  }
}

void TestBed::registerTestCase(const std::string &name, TestCaseFactory *factory)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_testCases.insert({{ name, TestCaseFactoryPtr(factory) }});
}

void TestBed::finishTestCase(TestCasePtr test)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_runningCases.erase(test);
}

void TestBed::loadScenario(Scenario *scenario)
{
  d->m_scenarios.insert(ScenarioPtr(scenario));
  scenario->setup();
}

void TestBed::loadTopology(const std::string &topologyFile)
{
  d->loadTopology(topologyFile);
}

void TestBed::runTest(const std::string &test)
{
  d->runTest(test);
}

void TestBed::scheduleTest(int time, const std::string &test)
{
  d->m_context.schedule(time, boost::bind(&TestBedPrivate::runTest, d, test));
}

void TestBed::scheduleTestEvery(int time, const std::string &test)
{
  d->m_context.schedule(time, boost::bind(&TestBedPrivate::runAndReschedule, d, time, test));
}

void TestBed::scheduleCall(int time, std::function<void()> handler)
{
  d->m_context.schedule(time, handler);
}

void TestBed::endScenarioAfter(int time)
{
  d->m_context.schedule(time, [this]() {
    RecursiveUniqueLock lock(d->m_mutex);
    d->m_snapshotHandler = nullptr;
    d->m_context.stop();
  });
}

void TestBed::snapshot(std::function<void()> handler)
{
  if (!handler)
    return;

  RecursiveUniqueLock lock(d->m_mutex);
  d->m_snapshotHandler = handler;
  d->m_context.stop();
}

int TestBed::time() const
{
  return (boost::posix_time::microsec_clock::universal_time() - d->m_timeStart).total_seconds();
}

}

}