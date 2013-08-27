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

  const std::vector<Partition> &getPartitions();

  std::mt19937 &rng();

  TestCasePtr callTestCase(const std::string &name);
private:
  DataSetBuffer &receive_(const std::string &dsName);

  void removeRunningTestCase();
public:
  /// Slave instance
  ControllerPrivate &m_controller;
  /// Test case instance
  TestCasePtr m_testCase;
  /// Received datasets
  std::unordered_map<std::string, DataSetBuffer> m_datasets;
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

  TestCasePtr test(const std::string &name,
                   typename TestCase::ArgumentList args);

  std::list<TestCasePtr> test(std::initializer_list<std::string> names);

  TestCasePtr testInBackground(const std::string &name);

  void signal(TestCasePtr test,
              const std::string &signal);

  const std::vector<Partition> &getPartitions() const;

  const std::vector<Partition::Node> &getNodes() const;

  void startNodes(const std::vector<Partition::Node> &nodes,
                  size_t offset,
                  size_t len);

  void startNode(const NodeIdentifier &nodeId);

  void stopNode(const NodeIdentifier &nodeId);
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
  /// Generated network partitions
  std::vector<Partition> m_partitions;
  /// Generated network nodes
  std::vector<Partition::Node> m_nodes;
  /// Mapping of nodes to partitions
  std::unordered_map<NodeIdentifier, size_t> m_partitionMap;
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

DataSetBuffer &ControllerTestCaseApi::receive_(const std::string &dsName)
{
  RecursiveUniqueLock lock(m_controller.m_mutex);
  
  // Check if a given dataset has been received
  auto it = m_datasets.find(dsName);
  if (it == m_datasets.end()) {
    BOOST_LOG_SEV(m_controller.m_logger, log::warning) << "Dataset '" << m_testCase->getName() << "/" << dsName << "' not received.";
    throw DataSetNotFound(dsName);
  }

  return it->second;
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

const std::vector<Partition> &ControllerTestCaseApi::getPartitions()
{
  return m_controller.m_partitions;
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

TestCasePtr ControllerScenarioApi::test(const std::string &name,
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

TestCasePtr ControllerScenarioApi::testInBackground(const std::string &name)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  return runTestCase(name, nullptr);
}

void ControllerScenarioApi::signal(TestCasePtr test,
                                   const std::string &signal)
{
  RecursiveUniqueLock lock(m_controller.m_mutex);

  if (test->isFinished())
    return;

  ScenarioPtr scenario = m_controller.m_scenario;
  test->signalFinished.connect(boost::bind(&Scenario::resume, scenario));

  BOOST_LOG(m_controller.m_logger) << "Sending signal '" << signal << "' to test '" << test->getName() << "'.";

  Protocol::SignalTestRequest request;
  request.set_test_id(test->getId());
  request.set_signal(signal);

  for (const Partition &partition : m_controller.m_partitions) {
    NodeIdentifier slaveId = partition.slave.nodeId();
    m_controller.q.rpc().call<Protocol::SignalTestRequest, Protocol::SignalTestResponse>(
      partition.slave.nodeId(),
      "Testbed.Simulation.SignalTest",
      request,
      nullptr,
      nullptr,
      m_controller.q.rpc().options()
                          .setTimeout(5)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }

  // Unlock the mutex before suspending, otherwise we will not be able to resume
  lock.unlock();
  scenario->suspend();
}

const std::vector<Partition> &ControllerScenarioApi::getPartitions() const
{
  return m_controller.m_partitions;
}

const std::vector<Partition::Node> &ControllerScenarioApi::getNodes() const
{
  return m_controller.m_nodes;
}

void ControllerScenarioApi::startNodes(const std::vector<Partition::Node> &nodes,
                                       size_t offset,
                                       size_t len)
{
  ScenarioPtr scenario = m_controller.m_scenario;
  std::vector<std::list<NodeIdentifier>> partitions;
  partitions.resize(m_controller.m_partitions.size());
  size_t nodeCount = 0;

  for (int i = offset; i - offset < len && i < nodes.size(); i++) {
    const Partition::Node &node = nodes.at(i);
    size_t partitionId = m_controller.m_partitionMap.at(node.contact.nodeId());
    partitions[partitionId].push_back(node.contact.nodeId());
    nodeCount++;
  }

  BOOST_LOG(m_controller.m_logger) << "Requesting to start " << nodeCount << " nodes.";

  // Contact proper slaves and instruct them to start the virtual nodes
  auto group = m_controller.q.rpc().group(boost::bind(&Scenario::resume, scenario));
  for (int i = 0; i < partitions.size(); i++) {
    std::list<NodeIdentifier> &nodes = partitions.at(i);
    const Partition &partition = m_controller.m_partitions.at(i);
    Protocol::StartNodesRequest request;

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
                          .setTimeout(5)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }

  scenario->suspend();
}

void ControllerScenarioApi::startNode(const NodeIdentifier &nodeId)
{
  // TODO: Request the slave to start a specific node
}

void ControllerScenarioApi::stopNode(const NodeIdentifier &nodeId)
{
  // TODO: Request the slave to stop a specific node
}

TestCasePtr ControllerScenarioApi::runTestCase(const std::string &name,
                                               std::function<void()> completion,
                                               typename TestCase::ArgumentList args)
{
  RecursiveUniqueLock lock(m_controller.m_mutex);

  TestCasePtr test = TestBed::getGlobalTestbed().createTestCase(name);
  if (!test) {
    BOOST_LOG_SEV(m_controller.m_logger, log::warning) << "Test case '" << name << "' not found.";
    return TestCasePtr();
  }

  if (completion)
    test->signalFinished.connect(completion);
  test->setArguments(args);

  // Create API instance
  boost::shared_ptr<ControllerTestCaseApi> api =
    boost::make_shared<ControllerTestCaseApi>(m_controller, test);
  api->m_rng.seed(m_controller.m_seed);

  // Call test case's pre-selection method
  test->preSelection(*api);

  // First obtain a list of virtual nodes that we should run the test on
  std::vector<SelectedPartition> selectedNodes;
  for (const Partition &partition : m_controller.m_partitions) {
    selectedNodes.push_back(SelectedPartition{ partition.index });
  }

  for (const Partition &partition : m_controller.m_partitions) {
    for (const Partition::Node &node : partition.nodes) {
      SelectedPartition::Node selected = test->selectNode(partition, node, *api);
      if (!selected.nodeId.isNull())
        selectedNodes[partition.index].nodes.push_back(selected);
    }
  }

  // Register the test case under running test cases
  BOOST_ASSERT(m_runningCases.find(test->getId()) == m_runningCases.end());
  m_runningCases[test->getId()] = RunningControllerTestCase{ test, api, selectedNodes, selectedNodes.size() };

  // Request slaves to run local portions of test cases and report back
  boost::shared_ptr<std::atomic<unsigned int>> pendingConfirms =
    boost::make_shared<std::atomic<unsigned int>>(selectedNodes.size());

  auto group = m_controller.q.rpc().group([this, pendingConfirms, test]() {
    if (*pendingConfirms != 0) {
      // Failed to run test case, abort whole simulation
      m_controller.q.abortSimulation();
      return;
    }

    // Test case now running on selected partitions
    BOOST_LOG_SEV(m_controller.m_logger, log::normal) << "Test case '" << test->getName() << "' now running.";
  });

  for (SelectedPartition &selected : selectedNodes) {
    Partition &partition = m_controller.m_partitions[selected.index];
    Protocol::RunTestRequest request;
    request.set_test_name(test->getName());
    request.set_test_id(test->getId());

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
        BOOST_LOG_SEV(m_controller.m_logger, log::normal) << "Test case running on " << slaveId.hex() << ".";
        (*pendingConfirms)--;
      },
      [this, slaveId](RpcErrorCode code, const std::string &msg) {
        BOOST_LOG_SEV(m_controller.m_logger, log::error) << "Failed to run test case on " << slaveId.hex() << ".";
      },
      m_controller.q.rpc().options()
                          .setTimeout(5)
                          .setChannelOptions(
                            MessageOptions().setContact(partition.slave)
                          )
    );
  }

  return test;
}

ControllerPrivate::ControllerPrivate(Controller &controller)
  : q(controller),
    m_logger(logging::keywords::channel = "cluster_controller")
{
}

Response<Protocol::DatasetResponse> ControllerPrivate::rpcDataset(const Protocol::DatasetRequest &request,
                                                                  const Message &msg,
                                                                  RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Obtain the running test
  auto &runningCases = m_scenarioApi->m_runningCases;
  auto it = runningCases.find(request.test_id());
  if (it == runningCases.end()) {
    BOOST_LOG_SEV(m_logger, log::warning) << "Received dataset for non-running test case!";
    throw RpcException(RpcErrorCode::BadRequest, "Specified test case is not running!");
  }

  RunningControllerTestCase &tc = it->second;
  BOOST_LOG_SEV(m_logger, log::normal) << "Received dataset '" << tc.testCase->getName() << "/" << request.ds_name() << "' from " << msg.originator().hex() << ".";

  // Store received (serialized) dataset in buffer
  DataSetBuffer &buffer = tc.api->m_datasets[request.ds_name()];
  buffer[std::make_tuple(msg.originator(), request.ds_instance())] << request.ds_data();

  return Protocol::DatasetResponse();
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
      ("cluster-master-id", po::value<std::string>(), "node identifier of cluster master")
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

  NodeIdentifier masterId;
  if (variables.count("cluster-master-id")) {
    masterId = NodeIdentifier(variables["cluster-master-id"].as<std::string>(), NodeIdentifier::Format::Hex);
    if (!masterId.isValid())
      throw ArgumentError("Invalid master node identifier specified!");
  } else {
    throw ArgumentError("Missing required --cluster-master-id option!");
  }

  d->m_masterContact = Contact(masterId);
  d->m_masterContact.addAddress(
    Address(
      variables["cluster-master-ip"].as<std::string>(),
      variables["cluster-master-port"].as<unsigned short>()
    )
  );
  d->m_master = rpc().service(masterId,
    rpc().options()
         .setTimeout(5)
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
      fail();
    }
  );
}

void Controller::run()
{
  // Create controller scenario API instance
  d->m_scenarioApi = boost::shared_ptr<ControllerScenarioApi>(new ControllerScenarioApi(context(), *d));

  // Register RPC methods
  rpc().registerMethod<Protocol::DatasetRequest, Protocol::DatasetResponse>("Testbed.Simulation.Dataset",
    boost::bind(&ControllerPrivate::rpcDataset, d, _1, _2, _3));

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

      BOOST_LOG_SEV(d->m_logger, log::normal) << "Initialized simulation with " << slaves.size() << " slaves.";

      // Load topology and assign partitions to slaves
      TopologyLoader loader(d->m_idGenType);
      loader.load(d->m_inputTopology);
      loader.partition(slaves);
      const auto &partitions = loader.getPartitions();
      d->m_partitions = partitions;
      d->m_unassignedPartitions = partitions.size();

      int i = 0;
      BOOST_LOG_SEV(d->m_logger, log::normal) << "Loaded topology with " << loader.getTopologySize() << " nodes.";
      for (const Partition &part : partitions) {
        BOOST_LOG_SEV(d->m_logger, log::normal) << "  [] Partition " << ++i << ": " << part.nodes.size() << " nodes";
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
          BOOST_LOG(d->m_logger) << "Scenario completed.";
          finishSimulation();
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

          for (const Contact &contact : node.peers) {
            *n->add_peers() = contact.toMessage();
          }

          d->m_nodes.push_back(node);
          d->m_partitionMap.insert({{ node.contact.nodeId(), part.index }});
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
               .setTimeout(5)
               .setChannelOptions(
                 MessageOptions().setContact(part.slave)
               )
        );
      }
    },
    [this](RpcErrorCode code, const std::string &msg) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to start simulation: " << msg;
      fail();
    }
  );
}

}

}
