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
#include "testbed/exceptions.h"
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
#include <boost/program_options.hpp>

namespace po = boost::program_options;

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
  /// Registered scenarios
  std::map<std::string, ScenarioPtr> m_scenarios;
  /// Snapshot handler
  std::function<void()> m_snapshotHandler;
  /// Simulation start time
  boost::posix_time::ptime m_timeStart;
  /// Program options
  po::variables_map m_options;
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
  if (!socialTopologyFile)
    throw TopologyLoadingFailed(filename);

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
  } catch (std::exception &e) {
    throw TopologyLoadingFailed(filename);
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

int TestBed::run(int argc, char **argv)
{
  // Setup program options
  po::options_description options;
  po::options_description globalOptions("Global testbed options");
  globalOptions.add_options()
    ("help", "show help message")
    ("scenario", po::value<std::string>(), "scenario to run")
    ("phy-ip", po::value<std::string>(), "physical ip address to use for nodes")
    ("phy-port", po::value<unsigned int>(), "physical starting port to use for nodes")
    ("out-dir", po::value<std::string>(), "directory for output files")
  ;
  options.add(globalOptions);

  // Setup per-scenario options
  for (ScenarioPtr scenario : d->m_scenarios | boost::adaptors::map_values) {
    scenario->init();
    options.add(scenario->options());
  }

  // Parse options
  po::variables_map &vm = d->m_options;
  try {
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);
  } catch (std::exception &e) {
    std::cout << "ERROR: There is an error in your invocation arguments!" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

  // Handle options
  if (vm.count("help")) {
    // Handle help option
    std::cout << "UNISPHERE Testbed" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

  if (vm.count("phy-ip") && vm.count("phy-port")) {
    setupPhyNetwork(vm["phy-ip"].as<std::string>(), vm["phy-port"].as<unsigned int>());
  } else {
    std::cout << "ERROR: Options --phy-ip and --phy-port not specified!" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

  if (!vm.count("out-dir")) {
    std::cout << "ERROR: Output directory not specified!" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

  if (vm.count("scenario")) {
    std::string scenario = vm["scenario"].as<std::string>();
    if (!runScenario(scenario)) {
      std::cout << "ERROR: Unable to run scenario '" << scenario << "'!" << std::endl;
      std::cout << options << std::endl;
      return 1;
    }
  } else {
    std::cout << "ERROR: Scenario not specified!" << std::endl;
    std::cout << options << std::endl;
    return 1;
  }

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

  return 0;
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

void TestBed::registerScenario(Scenario *scenario)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_scenarios.insert({{ scenario->name(), ScenarioPtr(scenario) }});
}

bool TestBed::runScenario(const std::string &scenario)
{
  RecursiveUniqueLock lock(d->m_mutex);
  auto it = d->m_scenarios.find(scenario);
  if (it == d->m_scenarios.end())
    return false;

  try {
    it->second->setup(d->m_options);
    return true;
  } catch (TestBedException &e) {
    std::cout << "ERROR: " << e.message() << std::endl;
    return false;
  }
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

std::string TestBed::getOutputDirectory() const
{
  return d->m_options["out-dir"].as<std::string>();
}

}

}