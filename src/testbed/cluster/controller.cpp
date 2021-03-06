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
#include "testbed/cluster/controller.h"
#include "testbed/cluster/slave_descriptor.h"
#include "testbed/cluster/topology_loader.h"
#include "testbed/test_bed.h"
#include "testbed/exceptions.h"
#include "testbed/scenario_api.h"
#include "identity/node_identifier.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"
#include "rpc/service.hpp"
#include "src/testbed/cluster/messages.pb.h"
#include "src/social/peer.pb.h"

#include <unordered_map>
#include <sstream>
#include <boost/archive/text_iarchive.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

template <typename ResponseType>
using Response = RpcResponse<InterplexRpcChannel, ResponseType>;

std::istream &operator>>(std::istream &is, TopologyLoader::IdGenerationType &type)
{
  std::string token;
  is >> token;
  if (token == "random")
    type = TopologyLoader::IdGenerationType::Random;
  else if (token == "consistent")
    type = TopologyLoader::IdGenerationType::Consistent;
  else
    throw boost::program_options::invalid_option_value("Invalid generation type");
  return is;
}

std::ostream &operator<<(std::ostream &os, const TopologyLoader::IdGenerationType &type)
{
  switch (type) {
    case TopologyLoader::IdGenerationType::Random: os << "random"; break;
    case TopologyLoader::IdGenerationType::Consistent: os << "consistent"; break;
    default: os << "unknown"; break;
  }
  return os;
}

class ControllerPrivate;

class ControllerTestCaseApi : public TestCaseApi {
public:
  ControllerTestCaseApi(ControllerPrivate &controller, TestCasePtr testCase);

  std::string getOutputFilename(const std::string &prefix,
                                const std::string &extension,
                                const std::string &marker);

  PartitionRange getPartitions();

  Partition::NodeRange getNodes() const;

  const Partition::Node &getNodeById(const NodeIdentifier &nodeId);

  std::mt19937 &rng();

  TestCasePtr callTestCase(const std::string &name);

  void setGlobalArguments(const boost::property_tree::ptree &args);

  DataSet dataset(const std::string &name);

  DataSet dataset(TestCasePtr testCase, const std::string &name);
private:
  void removeRunningTestCase();
public:
  /// Slave instance
  ControllerPrivate &m_controller;
  /// Test case instance
  TestCasePtr m_testCase;
  /// Global test case arguments
  boost::property_tree::ptree m_globalArgs;
  /// Random number generator
  std::mt19937 m_rng;
};

struct RunningControllerTestCase {
  /// Test case instance
  TestCasePtr testCase;
  /// API instance
  boost::shared_ptr<ControllerTestCaseApi> api;
  /// Partitions this test is running on
  std::vector<SelectedPartition> partitions;
  /// Number of partitions pending finish
  size_t pendingFinishes;
};

class ControllerScenarioApi : public ScenarioApi {
public:
  ControllerScenarioApi(Context &context,
                        ControllerPrivate &controller);

  void wait(int timeout);

  TestCasePtr test_(const std::string &name,
                    typename TestCase::ArgumentList args);

  std::list<TestCasePtr> test(std::initializer_list<std::string> names);

  TestCasePtr testInBackground_(const std::string &name,
                                typename TestCase::ArgumentList args);

  void signal(TestCasePtr test,
              const std::string &signal);

  PartitionRange getPartitions() const;

  Partition::NodeRange getNodes() const;

  void startNodes(const Partition::NodeRange &nodes);

  void startNode(const NodeIdentifier &nodeId);

  void stopNodes(const Partition::NodeRange &nodes);

  void stopNode(const NodeIdentifier &nodeId);

  std::mt19937 &rng();

  void mark(const std::string &marker);

  std::string getOutputFilename(const std::string &prefix,
                                const std::string &extension,
                                const std::string &marker);
public:
  TestCasePtr runTestCase(const std::string &name,
                          std::function<void()> completion,
                          typename TestCase::ArgumentList args = TestCase::ArgumentList());
public:
  /// Context
  Context &m_context;
  /// Controller
  ControllerPrivate &m_controller;
  /// Running test cases
  std::unordered_map<TestCase::Identifier, RunningControllerTestCase> m_runningCases;
  /// Random number generator
  std::mt19937 m_rng;
};

class ControllerPrivate {
public:
  ControllerPrivate(Controller &controller);

  Response<Protocol::DatasetResponse> rpcDataset(const Protocol::DatasetRequest &request,
                                                 const Message &msg,
                                                 RpcId rpcId);

  Response<Protocol::TestDoneResponse> rpcTestDone(const Protocol::TestDoneRequest &request,
                                                   const Message &msg,
                                                   RpcId rpcId);
public:
  UNISPHERE_DECLARE_PUBLIC(Controller)

  /// Mutex to protect the data structure
  std::recursive_mutex m_mutex;
  /// Logger instance
  Logger m_logger;
  /// Master contact
  Contact m_masterContact;
  /// Master service
  RpcService<InterplexRpcChannel> m_master;
  /// Input topology filename
  std::string m_inputTopology;
  /// Identifier generation type
  TopologyLoader::IdGenerationType m_idGenType;
  /// Topology loader
  TopologyLoader m_topology;
  /// Number of partitions pending assignment
  size_t m_unassignedPartitions;
  /// Seed value
  std::uint32_t m_seed;
  /// Output directory
  std::string m_outputDirectory;
  /// Simulation start time
  boost::posix_time::ptime m_simulationStartTime;
  /// Active scenario
  ScenarioPtr m_scenario;
  /// Scenario API instance
  boost::shared_ptr<ControllerScenarioApi> m_scenarioApi;
};

ControllerTestCaseApi::ControllerTestCaseApi(ControllerPrivate &controller,
                                             TestCasePtr testCase)
  : m_controller(controller),
    m_testCase(testCase)
{
}

DataSet ControllerTestCaseApi::dataset(const std::string &name)
{
  return DataSet(m_testCase->getIdString(), name);
}

DataSet ControllerTestCaseApi::dataset(TestCasePtr testCase, const std::string &name)
{
  return DataSet(testCase->getIdString(), name);
}

std::string ControllerTestCaseApi::getOutputFilename(const std::string &prefix,
                                                     const std::string &extension,
                                                     const std::string &marker)
{
  if (m_controller.m_outputDirectory.empty())
    return std::string();

  return (
    boost::format("%s/%s-%s%s-%05i.%s")
      % m_controller.m_outputDirectory
      % boost::algorithm::replace_all_copy(m_testCase->getName(), "/", "-")
      % boost::algorithm::replace_all_copy(prefix, "/", "-")
      % (marker.empty() ? "" : "-" + marker)
      % (boost::posix_time::microsec_clock::universal_time() - m_controller.m_simulationStartTime).total_seconds()
      % extension
  ).str();
}

PartitionRange ControllerTestCaseApi::getPartitions()
{
  return m_controller.m_topology.getPartitions();
}

Partition::NodeRange ControllerTestCaseApi::getNodes() const
{
  return m_controller.m_topology.getNodes(TopologyLoader::TraversalOrder::Unordered);
}

const Partition::Node &ControllerTestCaseApi::getNodeById(const NodeIdentifier &nodeId)
{
  return m_controller.m_topology.getNodeById(nodeId);
}

std::mt19937 &ControllerTestCaseApi::rng()
{
  return m_rng;
}

TestCasePtr ControllerTestCaseApi::callTestCase(const std::string &name)
{
  TestCasePtr test = m_controller.m_scenarioApi->runTestCase(name, nullptr);
  m_testCase->addChild(test);
  return test;
}

void ControllerTestCaseApi::setGlobalArguments(const boost::property_tree::ptree &args)
{
  RecursiveUniqueLock lock(m_controller.m_mutex);
  m_globalArgs = args;
}

void ControllerTestCaseApi::removeRunningTestCase()
{
  RecursiveUniqueLock lock(m_controller.m_mutex);
  m_controller.m_scenarioApi->m_runningCases.erase(m_testCase->getId());
}

ControllerScenarioApi::ControllerScenarioApi(Context &context,
                                             ControllerPrivate &controller)
  : m_context(context),
    m_controller(controller)
{
}

void ControllerScenarioApi::wait(int timeout)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  m_context.schedule(timeout, boost::bind(&Scenario::resume, scenario));
  scenario->suspend();
}

TestCasePtr ControllerScenarioApi::test_(const std::string &name,
                                         typename TestCase::ArgumentList args)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  TestCasePtr test = runTestCase(name, boost::bind(&Scenario::resume, scenario), args);
  if (test) {
    // Suspend execution while the test is running
    scenario->suspend();
  }
  return test;
}

std::list<TestCasePtr> ControllerScenarioApi::test(std::initializer_list<std::string> names)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  std::list<TestCasePtr> tests;
  boost::shared_ptr<std::atomic<unsigned int>> pendingTests =
    boost::make_shared<std::atomic<unsigned int>>(names.size());

  auto testCompletionHandler = [scenario, pendingTests]() {
    (*pendingTests)--;
    if (*pendingTests == 0)
      scenario->resume();
  };

  bool suspend = false;
  for (const std::string &name : names) {
    TestCasePtr test = runTestCase(name, testCompletionHandler);
    if (!test)
      m_context.schedule(0, testCompletionHandler);
    else
      suspend = true;
    tests.push_back(test);
  }

  if (suspend) {
    // Suspend execution while tests are running
    scenario->suspend();
  }
  return tests;
}

TestCasePtr ControllerScenarioApi::testInBackground_(const std::string &name,
                                                     typename TestCase::ArgumentList args)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  return runTestCase(name, nullptr, args);
}

void ControllerScenarioApi::signal(TestCasePtr test,
                                   const std::string &signal)
{
  RecursiveUniqueLock lock(m_controller.m_mutex);

  if (test->isFinished())
    return;

  ScenarioPtr scenario = m_controller.m_scenario;
  BOOST_LOG(m_controller.m_logger) << "Sending signal '" << signal << "' to test '" << test->getName() << "'.";

  Protocol::SignalTestRequest request;
  request.set_test_id(test->getId());
  request.set_signal(signal);

  auto group = m_controller.q.rpc().group(boost::bind(&Scenario::resume, scenario));
  for (const Partition &partition : m_controller.m_topology.getPartitions()) {
    if (partition.nodes.empty())
      continue;

    NodeIdentifier slaveId = partition.slave.nodeId();
    group->call<Protocol::SignalTestRequest, Protocol::SignalTestResponse>(
      partition.slave.nodeId(),
      "Testbed.Simulation.SignalTest",
      request,
      nullptr,
      nullptr,
      m_controller.q.rpc().options()
                          .setTimeout(30)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }
  group->start();

  // Unlock the mutex before suspending, otherwise we will not be able to resume
  lock.unlock();
  scenario->suspend();
}

PartitionRange ControllerScenarioApi::getPartitions() const
{
  return m_controller.m_topology.getPartitions();
}

Partition::NodeRange ControllerScenarioApi::getNodes() const
{
  return m_controller.m_topology.getNodes(TopologyLoader::TraversalOrder::BFS);
}

void ControllerScenarioApi::startNodes(const Partition::NodeRange &nodes)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  std::vector<std::list<NodeIdentifier>> partitions;
  partitions.resize(m_controller.m_topology.getPartitions().size());
  size_t nodeCount = 0;

  for (const Partition::Node &node : nodes) {
    partitions[node.partition].push_back(node.contact.nodeId());
    nodeCount++;
  }

  if (!nodeCount)
    return;

  BOOST_LOG(m_controller.m_logger) << "Requesting to start " << nodeCount << " node(s).";

  // Contact proper slaves and instruct them to start the virtual nodes
  auto group = m_controller.q.rpc().group(boost::bind(&Scenario::resume, scenario));
  for (int i = 0; i < partitions.size(); i++) {
    std::list<NodeIdentifier> &nodes = partitions.at(i);
    const Partition &partition = m_controller.m_topology.getPartitions().at(i);
    Protocol::StartNodesRequest request;
    if (nodes.empty())
      continue;

    for (const NodeIdentifier &nodeId : nodes) {
      request.add_node_ids(nodeId.raw());
    }

    NodeIdentifier slaveId = partition.slave.nodeId();
    group->call<Protocol::StartNodesRequest, Protocol::StartNodesResponse>(
      partition.slave.nodeId(),
      "Testbed.Simulation.StartNodes",
      request,
      nullptr,
      nullptr,
      m_controller.q.rpc().options()
                          .setTimeout(30)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }
  group->start();

  scenario->suspend();
}

void ControllerScenarioApi::startNode(const NodeIdentifier &nodeId)
{
  // TODO: Request the slave to start a specific node
}

void ControllerScenarioApi::stopNodes(const Partition::NodeRange &nodes)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  std::vector<std::list<NodeIdentifier>> partitions;
  partitions.resize(m_controller.m_topology.getPartitions().size());
  size_t nodeCount = 0;

  for (const Partition::Node &node : nodes) {
    partitions[node.partition].push_back(node.contact.nodeId());
    nodeCount++;
  }

  if (!nodeCount)
    return;

  BOOST_LOG(m_controller.m_logger) << "Requesting to stop " << nodeCount << " node(s).";

  // Contact proper slaves and instruct them to stop the virtual nodes
  auto group = m_controller.q.rpc().group(boost::bind(&Scenario::resume, scenario));
  for (int i = 0; i < partitions.size(); i++) {
    std::list<NodeIdentifier> &nodes = partitions.at(i);
    const Partition &partition = m_controller.m_topology.getPartitions().at(i);
    Protocol::StopNodesRequest request;
    if (nodes.empty())
      continue;

    for (const NodeIdentifier &nodeId : nodes) {
      request.add_node_ids(nodeId.raw());
    }

    NodeIdentifier slaveId = partition.slave.nodeId();
    group->call<Protocol::StopNodesRequest, Protocol::StopNodesResponse>(
      partition.slave.nodeId(),
      "Testbed.Simulation.StopNodes",
      request,
      nullptr,
      nullptr,
      m_controller.q.rpc().options()
                          .setTimeout(30)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }
  group->start();

  scenario->suspend();
}

void ControllerScenarioApi::stopNode(const NodeIdentifier &nodeId)
{
  BOOST_LOG(m_controller.m_logger) << "Requesting to stop node '" << nodeId.hex() << "'.";

  // Obtain the node descriptor
  const Partition::Node &node = m_controller.m_topology.getNodeById(nodeId);
  if (node.contact.isNull()) {
    BOOST_LOG_SEV(m_controller.m_logger, log::error) << "Failed to find node '" << nodeId.hex() << "' to stop.";
    return;
  }

  stopNodes(std::list<Partition::Node>({node}));
}

std::mt19937 &ControllerScenarioApi::rng()
{
  return m_rng;
}

std::string ControllerScenarioApi::getOutputFilename(const std::string &prefix,
                                                     const std::string &extension,
                                                     const std::string &marker)
{
  if (m_controller.m_outputDirectory.empty())
    return std::string();

  return (
    boost::format("%s/%s%s-%05i.%s")
      % m_controller.m_outputDirectory
      % boost::algorithm::replace_all_copy(prefix, "/", "-")
      % (marker.empty() ? "" : "-" + marker)
      % (boost::posix_time::microsec_clock::universal_time() - m_controller.m_simulationStartTime).total_seconds()
      % extension
  ).str();
}

void ControllerScenarioApi::mark(const std::string &marker)
{
  std::ofstream file;
  file.open(getOutputFilename("marker", "csv", marker));
  file << "ts\n";
  file << (boost::posix_time::microsec_clock::universal_time() - m_controller.m_simulationStartTime).total_seconds();
  file << "\n";

  BOOST_LOG(m_controller.m_logger) << "Marker '" << marker << "' reached.";
}

TestCasePtr ControllerScenarioApi::runTestCase(const std::string &name,
                                               std::function<void()> completion,
                                               typename TestCase::ArgumentList args)
{
  TestCasePtr test = TestBed::getGlobalTestbed().createTestCase(name, args);
  if (!test) {
    BOOST_LOG_SEV(m_controller.m_logger, log::warning) << "Test case '" << name << "' not found.";
    return TestCasePtr();
  }

  BOOST_LOG(m_controller.m_logger) << "Starting test case '" << test->getName() << "'.";

  if (completion)
    test->signalFinished.connect(completion);

  // Create API instance
  boost::shared_ptr<ControllerTestCaseApi> api =
    boost::make_shared<ControllerTestCaseApi>(m_controller, test);
  api->m_rng.seed(m_controller.m_seed);

  // Call test case's pre-selection method
  test->preSelection(*api);

  // First obtain a list of virtual nodes that we should run the test on
  const auto &partitions = m_controller.m_topology.getPartitions();
  std::vector<SelectedPartition> selectedNodes;
  for (const Partition &partition : partitions) {
    selectedNodes.push_back(SelectedPartition{ partition.index });
  }

  size_t selectedPartitions = 0;
  for (const Partition &partition : partitions) {
    for (const Partition::Node &node : partition.nodes) {
      SelectedPartition::Node selected = test->selectNode(partition, node, *api);
      if (!selected.nodeId.isNull())
        selectedNodes[partition.index].nodes.push_back(selected);
    }

    if (selectedNodes[partition.index].nodes.size() > 0)
      selectedPartitions++;
  }

  // If the test case is not scheduled to run on any partition, finish it immediately
  if (!selectedPartitions) {
    BOOST_LOG(m_controller.m_logger) << "Test case '" << test->getName() << "' not scheduled to run on any nodes.";
    test->tryComplete(*api);
    return test;
  }

  // Register the test case under running test cases
  {
    RecursiveUniqueLock lock(m_controller.m_mutex);
    BOOST_ASSERT(m_runningCases.find(test->getId()) == m_runningCases.end());
    m_runningCases[test->getId()] = RunningControllerTestCase{ test, api, selectedNodes, selectedPartitions };
  }

  // Request slaves to run local portions of test cases and report back
  boost::shared_ptr<std::atomic<unsigned int>> pendingConfirms =
    boost::make_shared<std::atomic<unsigned int>>(selectedPartitions);

  auto group = m_controller.q.rpc().group([this, pendingConfirms, test]() {
    if (*pendingConfirms != 0) {
      // Failed to run test case, abort whole simulation
      m_controller.q.abortSimulation();
      return;
    }

    // Test case now running on selected partitions
    BOOST_LOG(m_controller.m_logger) << "Test case '" << test->getName() << "' now running.";
  });

  for (SelectedPartition &selected : selectedNodes) {
    if (!selected.nodes.size())
      continue;

    const Partition &partition = partitions[selected.index];
    Protocol::RunTestRequest request;
    request.set_test_name(test->getName());
    request.set_test_id(test->getId());

    // Serialize global arguments to JSON string
    {
      std::ostringstream buffer;
      boost::property_tree::write_json(buffer, api->m_globalArgs, false);
      request.set_test_arguments(buffer.str());
    }

    for (SelectedPartition::Node &node : selected.nodes) {
      Protocol::RunTestRequest::Node *pnode = request.add_nodes();
      pnode->set_id(node.nodeId.raw());

      // Serialize arguments to JSON string
      std::ostringstream buffer;
      boost::property_tree::write_json(buffer, node.args, false);
      pnode->set_arguments(buffer.str());
    }

    NodeIdentifier slaveId = partition.slave.nodeId();
    group->call<Protocol::RunTestRequest, Protocol::RunTestResponse>(
      partition.slave.nodeId(),
      "Testbed.Simulation.RunTest",
      request,
      [this, slaveId, pendingConfirms](const Protocol::RunTestResponse &response, const Message&) {
        BOOST_LOG(m_controller.m_logger) << "Test case running on " << slaveId.hex() << ".";
        (*pendingConfirms)--;
      },
      [this, slaveId](RpcErrorCode code, const std::string &msg) {
        BOOST_LOG_SEV(m_controller.m_logger, log::error) << "Failed to run test case on " << slaveId.hex() << " (" << msg << ").";
      },
      m_controller.q.rpc().options()
                          .setTimeout(30)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }

  BOOST_LOG(m_controller.m_logger) << "Test case '" << test->getName() << "' scheduled to start.";
  group->start();

  return test;
}

ControllerPrivate::ControllerPrivate(Controller &controller)
  : q(controller),
    m_logger(logging::keywords::channel = "cluster_controller")
{
}

Response<Protocol::TestDoneResponse> ControllerPrivate::rpcTestDone(const Protocol::TestDoneRequest &request,
                                                                    const Message &msg,
                                                                    RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Obtain the running test
  auto &runningCases = m_scenarioApi->m_runningCases;
  auto it = runningCases.find(request.test_id());
  if (it == runningCases.end()) {
    BOOST_LOG_SEV(m_logger, log::warning) << "Received finish() for non-running test case!";
    throw RpcException(RpcErrorCode::BadRequest, "Specified test case is not running!");
  }

  RunningControllerTestCase &tc = it->second;
  BOOST_LOG_SEV(m_logger, log::normal) << "Test case '" << tc.testCase->getName() << "' finished on " << msg.originator().hex() << ".";

  // When we have received finish notifications from all slaves, try to finish locally
  if (--tc.pendingFinishes == 0)
    tc.testCase->tryComplete(*tc.api);

  return Protocol::TestDoneResponse();
}

Controller::Controller()
  : ClusterNode(),
    d(new ControllerPrivate(*this))
{
}

void Controller::setupOptions(int argc,
                              char **argv,
                              po::options_description &options,
                              po::variables_map &variables)
{
  TestBed &testbed = TestBed::getGlobalTestbed();

  if (variables.empty()) {
    ClusterNode::setupOptions(argc, argv, options, variables);

    // Cluster-related controller options
    po::options_description clusterOpts("Slave Cluster Options");
    clusterOpts.add_options()
      ("cluster-master-ip", po::value<std::string>(), "IP address of cluster master")
      ("cluster-master-port", po::value<unsigned short>()->default_value(8471), "port of cluster master")
      ("cluster-master-pub-key", po::value<std::string>(), "public key of cluster master")
    ;
    options.add(clusterOpts);

    // Generate a list of all available scenarios
    std::stringstream scenarios;
    scenarios << "scenario to run\n"
              << "\n"
              << "Available scenarios:\n";

    for (ScenarioPtr scenario : testbed.scenarios() | boost::adaptors::map_values) {
      scenarios << "  " << scenario->name() << "\n";
    }

    // Testbed options on controller node
    po::options_description testbedOpts("Testbed Options");
    testbedOpts.add_options()
      ("topology", po::value<std::string>(), "input topology in GraphML format")
      ("scenario", po::value<std::string>(), scenarios.str().data())
      ("out-dir", po::value<std::string>(), "directory for output files")
      ("id-gen", po::value<TopologyLoader::IdGenerationType>()->default_value(TopologyLoader::IdGenerationType::Consistent),
        "id generation type [random, consistent]")
      ("seed", po::value<std::uint32_t>()->default_value(0), "seed for the basic RNG")
      ("max-runtime", po::value<unsigned int>()->default_value(0), "maximum runtime in seconds (0 = unlimited)")
    ;
    options.add(testbedOpts);
    return;
  }

  // Process local options
  ClusterNode::setupOptions(argc, argv, options, variables);

  // Validate options
  if (!variables.count("cluster-master-ip")) {
    throw ArgumentError("Missing required --cluster-master-ip option!");
  } else if (!variables.count("cluster-master-port")) {
    throw ArgumentError("Missing required --cluster-master-port option!");
  }

  PublicPeerKey masterKey;
  if (variables.count("cluster-master-pub-key")) {
    try {
      masterKey = PublicPeerKey(variables["cluster-master-pub-key"].as<std::string>(),
        PublicPeerKey::Format::Base64);
    } catch (KeyDecodeFailed &e) {
      throw ArgumentError("Invalid master node public key specified!");
    }
  } else {
    throw ArgumentError("Missing required --cluster-master-pub-key option!");
  }

  d->m_masterContact = Contact(masterKey);
  d->m_masterContact.addAddress(
    Address(
      variables["cluster-master-ip"].as<std::string>(),
      variables["cluster-master-port"].as<unsigned short>()
    )
  );
  d->m_master = rpc().service(masterKey.nodeId(),
    rpc().options()
         .setTimeout(30)
         .setChannelOptions(
           MessageOptions().setContact(d->m_masterContact)
         )
  );

  // Process testbed options
  if (variables.count("topology")) {
    d->m_inputTopology = variables["topology"].as<std::string>();
  } else {
    throw ArgumentError("Missing required --topology option!");
  }

  ScenarioPtr scenario;
  if (variables.count("scenario")) {
    scenario = testbed.getScenario(variables["scenario"].as<std::string>());
    if (!scenario)
      throw ArgumentError("The specified scenario is not registered!");
  } else {
    throw ArgumentError("Missing required --scenario option!");
  }

  d->m_idGenType = variables["id-gen"].as<TopologyLoader::IdGenerationType>();
  d->m_seed = variables["seed"].as<std::uint32_t>();
  if (variables.count("out-dir")) {
    // Validate the output directory
    try {
      d->m_outputDirectory = boost::filesystem::canonical(variables["out-dir"].as<std::string>()).string();
    } catch (boost::filesystem::filesystem_error&) {
      throw ArgumentError("Invalid output directory specified!");
    }
  }

  scenario->initialize(argc, argv, options);
  d->m_scenario = scenario;
}

void Controller::abortSimulation()
{
  // Request the master to abort the simulation
  d->m_master.call<Protocol::AbortRequest, Protocol::AbortResponse>(
    "Testbed.Cluster.Abort",
    Protocol::AbortRequest(),
    [this](const Protocol::AbortResponse &response, const Message&) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Simulation aborted.";
      stop();
    },
    [this](RpcErrorCode code, const std::string &msg) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to abort simulation: " << msg;
      fail();
    }
  );
}

void Controller::finishSimulation()
{
  // Request the master to abort the simulation
  // TODO: Should finishSimulation use a separate method call?
  d->m_master.call<Protocol::AbortRequest, Protocol::AbortResponse>(
    "Testbed.Cluster.Abort",
    Protocol::AbortRequest(),
    [this](const Protocol::AbortResponse &response, const Message&) {
      BOOST_LOG(d->m_logger) << "Simulation finished.";
      stop();
    },
    [this](RpcErrorCode code, const std::string &msg) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to finish simulation: " << msg;
      stop();
    }
  );
}

void Controller::run()
{
  // Create controller scenario API instance
  d->m_scenarioApi = boost::make_shared<ControllerScenarioApi>(context(), *d);
  d->m_scenarioApi->m_rng.seed(d->m_seed);

  // Register RPC methods
  rpc().registerMethod<Protocol::TestDoneRequest, Protocol::TestDoneResponse>("Testbed.Simulation.TestDone",
    boost::bind(&ControllerPrivate::rpcTestDone, d, _1, _2, _3));

  BOOST_LOG_SEV(d->m_logger, log::normal) << "Cluster controller initialized.";

  // Get slave list from master and start simulation so no new slaves can register
  d->m_master.call<Protocol::StartRequest, Protocol::StartResponse>(
    "Testbed.Cluster.Start",
    Protocol::StartRequest(),
    [this](const Protocol::StartResponse &response, const Message&) {
      // Prepare a list of slaves that we have available
      SlaveDescriptorMap slaves;
      for (int i = 0; i < response.slaves_size(); i++) {
        const Protocol::StartResponse::Slave &slave = response.slaves(i);
        Contact contact = Contact::fromMessage(slave.contact());
        slaves.insert({{ contact.nodeId(),
          SlaveDescriptor{ contact, slave.ip(), std::make_tuple(slave.port_start(), slave.port_end()) }
        }});
      }

      // Configure dataset storage connection string
      try {
        DataSetStorage &dss = TestBed::getGlobalTestbed().getDataSetStorage();
        dss.setConnectionString(response.dataset_storage_cs());
        dss.initialize();
        dss.clear();
        BOOST_LOG(d->m_logger) << "Dataset storage configured (cs=" << dss.getConnectionString().toString() << ").";
      } catch (ConnectionStringError &e) {
        BOOST_LOG_SEV(d->m_logger, log::error) << "Master sent us invalid dataset storage configuration!";
        abortSimulation();
        return;
      }

      BOOST_LOG_SEV(d->m_logger, log::normal) << "Initialized simulation with " << slaves.size() << " slaves.";

      // Load topology and assign partitions to slaves
      auto &loader = d->m_topology;
      loader.load(d->m_inputTopology);
      loader.partition(slaves, d->m_idGenType);
      const auto &partitions = loader.getPartitions();
      d->m_unassignedPartitions = partitions.size();

      int i = 0;
      BOOST_LOG_SEV(d->m_logger, log::normal) << "Loaded topology with " << loader.getTopologySize() << " nodes.";
      for (const Partition &part : partitions) {
        size_t links = 0;
        for (const auto &node : part.nodes) {
          links += node.peers.size();
        }

        BOOST_LOG_SEV(d->m_logger, log::normal) << "  [] Partition " << ++i << ": " << part.nodes.size() << " nodes, " << links << " links";
      }

      // Instruct each slave to create its own partition
      auto group = rpc().group([this]() {
        // Called after all RPC calls to slaves complete
        if (d->m_unassignedPartitions != 0) {
          BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to assign all partitions, aborting.";
          abortSimulation();
          return;
        }

        BOOST_LOG_SEV(d->m_logger, log::normal) << "Partitions assigned. Starting scenario '" << d->m_scenario->name() << "'.";
        d->m_simulationStartTime = boost::posix_time::microsec_clock::universal_time();
        d->m_scenario->signalFinished.connect([this]() {
          RecursiveUniqueLock lock(d->m_mutex);
          if (d->m_scenarioApi->m_runningCases.empty()) {
            BOOST_LOG(d->m_logger) << "Scenario completed.";
            finishSimulation();
          } else {
            BOOST_LOG(d->m_logger) << "Scenario completed, waiting for remaining test cases.";

            // Wait for all test cases to complete
            for (const auto &p : d->m_scenarioApi->m_runningCases) {
              p.second.testCase->signalFinished.connect([this]() {
                RecursiveUniqueLock lock(d->m_mutex);
                if (d->m_scenarioApi->m_runningCases.empty()) {
                  BOOST_LOG(d->m_logger) << "Remaining tests completed.";
                  finishSimulation();
                }
              });
            }
          }
        });
        d->m_scenario->start(*d->m_scenarioApi);
      });

      for (const Partition &part : partitions) {
        Protocol::AssignPartitionRequest request;
        request.set_num_global_nodes(loader.getTopologySize());
        request.set_seed(d->m_seed);

        for (const auto &node : part.nodes) {
          Protocol::AssignPartitionRequest::Node *n = request.add_nodes();
          n->set_name(node.name);
          *n->mutable_contact() = node.contact.toMessage();
          n->set_public_key(node.privateKey.raw());
          n->set_private_key(node.privateKey.privateRaw());

          for (const Peer &peer : node.peers) {
            UniSphere::Protocol::Peer *p = n->add_peers();
            *p->mutable_contact() = peer.contact().toMessage();
          }
        }

        NodeIdentifier slaveId = part.slave.nodeId();
        group->call<Protocol::AssignPartitionRequest, Protocol::AssignPartitionResponse>(
          part.slave.nodeId(),
          "Testbed.Cluster.AssignPartition",
          request,
          [this, slaveId](const Protocol::AssignPartitionResponse &response, const Message&) {
            BOOST_LOG_SEV(d->m_logger, log::normal) << "Assigned partition to " << slaveId.hex() << ".";
            d->m_unassignedPartitions--;
          },
          [this, slaveId](RpcErrorCode code, const std::string &msg) {
            BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to assign partition to " << slaveId.hex() << ": " << msg;
          },
          rpc().options()
               .setTimeout(30)
               .setChannelOptions(
                 MessageOptions().setContact(part.slave)
               )
        );
      }
      group->start();
    },
    [this](RpcErrorCode code, const std::string &msg) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to start simulation: " << msg;
      fail();
    }
  );
}

}

}
