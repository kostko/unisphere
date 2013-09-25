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
#include "testbed/cluster/slave.h"
#include "testbed/exceptions.h"
#include "testbed/test_bed.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"
#include "rpc/service.hpp"
#include "src/testbed/cluster/messages.pb.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <atomic>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

template <typename ResponseType>
using Response = RpcResponse<InterplexRpcChannel, ResponseType>;

template <typename ResponseType>
using DeferredResponse = RpcDeferredResponse<InterplexRpcChannel, ResponseType>;

class SlavePrivate;

class SlaveTestCaseApi : public TestCaseApi {
public:
  SlaveTestCaseApi(SlavePrivate &slave, TestCasePtr testCase);

  void finishNow();

  std::mt19937 &rng();

  void defer(std::function<void()> fun, int timeout);

  std::uint32_t getTime();
private:
  void send_(const std::string &dsName,
             std::istream &dsData);
public:
  /// Slave instance
  SlavePrivate &m_slave;
  /// Test case instance
  TestCasePtr m_testCase;
  /// Random number generator
  std::mt19937 m_rng;
  /// Dataset instance counter
  std::uint32_t m_datasetInstance;
  /// Dataset buffer
  std::vector<char> m_datasetBuffer;
};

struct RunningSlaveTestCase {
  /// Test case instance
  TestCasePtr testCase;
  /// API instance
  boost::shared_ptr<SlaveTestCaseApi> api;
};

class SlavePrivate {
public:
  SlavePrivate(Slave &slave);

  Response<Protocol::AssignPartitionResponse> rpcAssignPartition(const Protocol::AssignPartitionRequest &request,
                                                                 const Message &msg,
                                                                 RpcId rpcId);

  Response<Protocol::AbortResponse> rpcAbort(const Protocol::AbortRequest &request,
                                             const Message &msg,
                                             RpcId rpcId);

  Response<Protocol::RunTestResponse> rpcRunTest(const Protocol::RunTestRequest &request,
                                                 const Message &msg,
                                                 RpcId rpcId);

  void rpcSignalTest(const Protocol::SignalTestRequest &request,
                     const Message &msg,
                     RpcId rpcId,
                     const DeferredResponse<Protocol::SignalTestResponse> &response);

  Response<Protocol::StartNodesResponse> rpcStartNodes(const Protocol::StartNodesRequest &request,
                                                       const Message &msg,
                                                       RpcId rpcId);
public:
  UNISPHERE_DECLARE_PUBLIC(Slave)

  /// Mutex to protect the data structure
  std::recursive_mutex m_mutex;
  /// Logger instance
  Logger m_logger;
  /// Master contact
  Contact m_masterContact;
  /// Master service
  RpcService<InterplexRpcChannel> m_master;
  /// Controller service
  RpcService<InterplexRpcChannel> m_controller;
  /// Simulation IP address
  std::string m_simulationIp;
  /// Simulation port range
  std::tuple<unsigned short, unsigned short> m_simulationPortRange;
  /// Simulation thread count
  size_t m_simulationThreads;
  /// Heartbeat timer
  boost::asio::deadline_timer m_heartbeatTimer;
  /// Master heartbeat timeout counter
  size_t m_masterMissedHeartbeats;
  /// Currently active simulation
  SimulationPtr m_simulation;
  /// Running test cases
  std::unordered_map<TestCase::Identifier, RunningSlaveTestCase> m_runningCases;
};

SlaveTestCaseApi::SlaveTestCaseApi(SlavePrivate &slave, TestCasePtr testCase)
  : m_slave(slave),
    m_testCase(testCase),
    m_datasetInstance(0)
{
}

void SlaveTestCaseApi::finishNow()
{
  RecursiveUniqueLock lock(m_slave.m_mutex);

  // Ensure that test case is destroyed after we exit
  auto it = m_slave.m_runningCases.find(m_testCase->getId());
  if (it == m_slave.m_runningCases.end()) {
    BOOST_LOG_SEV(m_slave.m_logger, log::error) << "Test case not found while finish()-ing!";
    return;
  }

  BOOST_LOG_SEV(m_slave.m_logger, log::normal) << "Test case '" << m_testCase->getName() << "' finished.";

  // Notify the controller that we are done with the test case
  Protocol::TestDoneRequest request;
  request.set_test_id(m_testCase->getId());

  // TODO: Should we do error handling when notification can't be done?
  m_slave.m_controller.call<Protocol::TestDoneRequest, Protocol::TestDoneResponse>(
    "Testbed.Simulation.TestDone",
    request,
    nullptr,
    nullptr
  );

  // Erase only after the above has finished as this will destroy the test case instance
  m_slave.m_runningCases.erase(it);
}

void SlaveTestCaseApi::send_(const std::string &dsName,
                             std::istream &dsData)
{
  RecursiveUniqueLock lock(m_slave.m_mutex);
  
  Protocol::DatasetRequest request;
  request.set_test_id(m_testCase->getId());
  request.set_ds_name(dsName);
  request.set_ds_instance(m_datasetInstance++);

  BOOST_LOG_SEV(m_slave.m_logger, log::normal) << "Sending dataset '" << m_testCase->getName() << "/" << dsName << "'.";

  m_datasetBuffer.resize(1048576);
  while (!dsData.eof()) {
    dsData.read(&m_datasetBuffer[0], m_datasetBuffer.size());
    request.set_ds_data(&m_datasetBuffer[0], dsData.gcount());

    // TODO: Should we do error handling when notification can't be done?
    m_slave.m_controller.call<Protocol::DatasetRequest, Protocol::DatasetResponse>(
      "Testbed.Simulation.Dataset",
      request,
      nullptr,
      nullptr
    );
  }
}

std::mt19937 &SlaveTestCaseApi::rng()
{
  return m_rng;
}

void SlaveTestCaseApi::defer(std::function<void()> fun, int timeout)
{
  SimulationSectionPtr section = m_slave.m_simulation->section();
  TestCaseWeakPtr testCase(m_testCase);
  section->execute([testCase, fun]() {
    // Do not execute the defered function when test case has already finished
    if (TestCasePtr tc = testCase.lock()) {
      if (tc->isFinished())
        return;

      fun();
    }
  });

  if (timeout > 0)
    section->schedule(timeout);
  else
    section->run();
}

std::uint32_t SlaveTestCaseApi::getTime()
{
  return m_slave.m_simulation->context().getCurrentTimestamp();
}

SlavePrivate::SlavePrivate(Slave &slave)
  : q(slave),
    m_logger(logging::keywords::channel = "cluster_slave"),
    m_heartbeatTimer(q.context().service())
{
}

Response<Protocol::AssignPartitionResponse> SlavePrivate::rpcAssignPartition(const Protocol::AssignPartitionRequest &request,
                                                                             const Message &msg,
                                                                             RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);
  if (m_simulation)
    throw RpcException(RpcErrorCode::BadRequest, "Simulation is already running!");

  TestBed &testbed = TestBed::getGlobalTestbed();
  SimulationPtr simulation = testbed.createSimulation(request.seed(), m_simulationThreads, request.num_global_nodes());

  // Create virtual node instances for each node in the partition
  for (int i = 0; i < request.nodes_size(); i++) {
    auto &node = request.nodes(i);
    Contact contact = Contact::fromMessage(node.contact());
    std::list<Contact> peers;
    for (int j = 0; j < node.peers_size(); j++) {
      peers.push_back(Contact::fromMessage(node.peers(j)));
    }

    simulation->createNode(node.name(), contact, peers);
  }

  // Setup controller service
  m_controller = q.rpc().service(msg.originator(),
    q.rpc().options()
           .setTimeout(5)
           .setChannelOptions(
             MessageOptions().setContact(q.linkManager().getLinkContact(msg.originator()))
           )
  );

  // Setup currently running simulation
  m_simulation = simulation;
  m_simulation->run();

  return Protocol::AssignPartitionResponse();
}

Response<Protocol::AbortResponse> SlavePrivate::rpcAbort(const Protocol::AbortRequest &request,
                                                         const Message &msg,
                                                         RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);

  // When no simulation is running, abort always succeeds
  if (!m_simulation || m_simulation->isStopping())
    return Protocol::AbortResponse();

  // Note that the actual simulation will not get destroyed until the simulation stops
  BOOST_LOG_SEV(m_logger, log::warning) << "Aborting simulation as requested by master!";
  m_simulation->signalStopped.connect([&]() {
    RecursiveUniqueLock lock(m_mutex);
    BOOST_LOG_SEV(m_logger, log::normal) << "Simulation stopped.";
    m_simulation.reset();
    q.rejoinCluster();
  });
  m_simulation->stop();

  return Protocol::AbortResponse();
}

Response<Protocol::RunTestResponse> SlavePrivate::rpcRunTest(const Protocol::RunTestRequest &request,
                                                             const Message &msg,
                                                             RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Fail when no simulation is running
  if (!m_simulation || m_simulation->isStopping())
    throw RpcException(RpcErrorCode::BadRequest, "Simulation is not running!");

  // Create a new test case
  TestCasePtr test = TestBed::getGlobalTestbed().createTestCase(request.test_name());
  if (!test) {
    BOOST_LOG_SEV(m_logger, log::warning) << "Test case '" << request.test_name() << "' not found.";
    throw RpcException(RpcErrorCode::BadRequest, "Test case '" + request.test_name() + "' not found!");
  }
  test->setId(request.test_id());

  boost::shared_ptr<SlaveTestCaseApi> api = boost::make_shared<SlaveTestCaseApi>(*this, test);
  api->m_rng.seed(m_simulation->seed());
  m_runningCases.insert({{ test->getId(), RunningSlaveTestCase{ test, api } }});

  BOOST_LOG_SEV(m_logger, log::normal) << "Running test case '" << test->getName() << "' on " << request.nodes_size() << " nodes.";

  SimulationSectionPtr section = m_simulation->section();

  // Deserialize global test case parameters and schedule pre-run
  {
    boost::property_tree::ptree args;
    std::istringstream buffer(request.test_arguments());
    boost::property_tree::read_json(buffer, args);
    section->execute([this, test, api, args]() {
      test->preRunNodes(*api, args);
    });
  }

  // Run the test case on all specified nodes
  for (int i = 0; i < request.nodes_size(); i++) {
    const Protocol::RunTestRequest::Node &node = request.nodes(i);
    boost::property_tree::ptree args;
    std::istringstream buffer(node.arguments());
    boost::property_tree::read_json(buffer, args);

    try {
      // Schedule executions within the simulation
      section->execute(
        NodeIdentifier(node.id(), NodeIdentifier::Format::Raw),
        [this, test, api, args](VirtualNodePtr vnode) {
          test->runNode(*api, vnode, args);
        }
      );
    } catch (VirtualNodeNotFound &e) {
      BOOST_LOG_SEV(m_logger, log::error) << "Failed to run test case in simulation: virtual node not found.";
      throw RpcException(RpcErrorCode::BadRequest, "Failed to run test case in simulation: virtual node not found.");
    }
  }

  // Setup a completion handler
  section->signalFinished.connect([this, test, api]() {
    test->localNodesRunning(*api);

    // If test case is not yet finished, we transition it to running state; otherwise
    // process local test results and finish it immediately
    if (!test->isFinished()) {
      test->setState(TestCase::State::Running);
    } else {
      test->processLocalResults(*api);
      api->finishNow();
    }
  });

  section->run();

  return Protocol::RunTestResponse();
}

void SlavePrivate::rpcSignalTest(const Protocol::SignalTestRequest &request,
                                 const Message &msg,
                                 RpcId rpcId,
                                 const DeferredResponse<Protocol::SignalTestResponse> &response)
{
  RecursiveUniqueLock lock(m_mutex);

  // Fail when no simulation is running
  if (!m_simulation || m_simulation->isStopping())
    throw RpcException(RpcErrorCode::BadRequest, "Simulation is not running!");

  auto it = m_runningCases.find(request.test_id());
  if (it == m_runningCases.end())
    throw RpcException(RpcErrorCode::BadRequest, "Test case not found!");

  RunningSlaveTestCase &runningCase = it->second;
  BOOST_LOG(m_logger) << "Sending signal '" << request.signal() << "' to test '" << runningCase.testCase->getName() << "'.";

  SimulationSectionPtr section = m_simulation->section();
  section->execute([&runningCase, request, response]() {
    runningCase.testCase->signalReceived(*runningCase.api, request.signal());
    response.success();
  });
  section->run();
}

Response<Protocol::StartNodesResponse> SlavePrivate::rpcStartNodes(const Protocol::StartNodesRequest &request,
                                                                   const Message &msg,
                                                                   RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Fail when no simulation is running
  if (!m_simulation || m_simulation->isStopping())
    throw RpcException(RpcErrorCode::BadRequest, "Simulation is not running!");

  BOOST_LOG(m_logger) << "Starting " << request.node_ids_size() << " nodes.";

  SimulationSectionPtr section = m_simulation->section();
  for (int i = 0; i < request.node_ids_size(); i++) {
    section->execute(NodeIdentifier(request.node_ids(i), NodeIdentifier::Format::Raw),
      [](VirtualNodePtr vnode) { vnode->initialize(); });
  }
  section->run();

  return Protocol::StartNodesResponse();
}

Slave::Slave()
  : ClusterNode(),
    d(new SlavePrivate(*this))
{
}

void Slave::setupOptions(int argc,
                          char **argv,
                          po::options_description &options,
                          po::variables_map &variables)
{
  if (variables.empty()) {
    ClusterNode::setupOptions(argc, argv, options, variables);

    // Cluster-related slave options
    po::options_description local("Slave Cluster Options");
    local.add_options()
      ("cluster-master-ip", po::value<std::string>(), "IP address of cluster master")
      ("cluster-master-port", po::value<unsigned short>()->default_value(8471), "port of cluster master")
      ("cluster-master-id", po::value<std::string>(), "node identifier of cluster master")
    ;
    options.add(local);

    // Simulation-related slave options
    po::options_description simulation("Simulation Options");
    simulation.add_options()
      ("sim-ip", po::value<std::string>(), "IP address available for simulation")
      ("sim-port-start", po::value<unsigned short>(), "start of simulation port range")
      ("sim-port-end", po::value<unsigned short>(), "end of simulation port range")
      ("sim-threads", po::value<size_t>(), "number of simulation threads")
    ;
    options.add(simulation);
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

  if (!variables.count("sim-ip")) {
    throw ArgumentError("Missing required --sim-ip option!");
  } else if (!variables.count("sim-port-start")) {
    throw ArgumentError("Missing required --sim-port-start option!");
  } else if (!variables.count("sim-port-end")) {
    throw ArgumentError("Missing required --sim-port-end option!");
  } else if (!variables.count("sim-threads")) {
    throw ArgumentError("Missing required --sim-threads option!");
  }

  d->m_simulationIp = variables["sim-ip"].as<std::string>();
  d->m_simulationPortRange = std::make_tuple(
    variables["sim-port-start"].as<unsigned short>(),
    variables["sim-port-end"].as<unsigned short>()
  );
  d->m_simulationThreads = variables["sim-threads"].as<size_t>();

  if (std::get<0>(d->m_simulationPortRange) > std::get<1>(d->m_simulationPortRange)) {
    throw ArgumentError("Invalid port range specified!");
  }
}

void Slave::run()
{
  // Register RPC methods
  rpc().registerMethod<Protocol::AssignPartitionRequest, Protocol::AssignPartitionResponse>("Testbed.Cluster.AssignPartition",
    boost::bind(&SlavePrivate::rpcAssignPartition, d, _1, _2, _3));

  rpc().registerMethod<Protocol::AbortRequest, Protocol::AbortResponse>("Testbed.Cluster.Abort",
    boost::bind(&SlavePrivate::rpcAbort, d, _1, _2, _3));

  rpc().registerMethod<Protocol::RunTestRequest, Protocol::RunTestResponse>("Testbed.Simulation.RunTest",
    boost::bind(&SlavePrivate::rpcRunTest, d, _1, _2, _3));

  rpc().registerDeferredMethod<Protocol::SignalTestRequest, Protocol::SignalTestResponse>("Testbed.Simulation.SignalTest",
    boost::bind(&SlavePrivate::rpcSignalTest, d, _1, _2, _3, _4));

  rpc().registerMethod<Protocol::StartNodesRequest, Protocol::StartNodesResponse>("Testbed.Simulation.StartNodes",
    boost::bind(&SlavePrivate::rpcStartNodes, d, _1, _2, _3));

  BOOST_LOG_SEV(d->m_logger, log::normal) << "Cluster slave initialized.";

  joinCluster();
}

void Slave::joinCluster()
{
  Protocol::ClusterJoinRequest request;
  request.set_simulation_ip(d->m_simulationIp);
  request.set_simulation_port_start(std::get<0>(d->m_simulationPortRange));
  request.set_simulation_port_end(std::get<1>(d->m_simulationPortRange));

  d->m_master.call<Protocol::ClusterJoinRequest, Protocol::ClusterJoinResponse>(
    "Testbed.Cluster.Join",
    request,
    [this](const Protocol::ClusterJoinResponse &response, const Message&) {
      // Check if registration succeeded
      if (!response.registered()) {
        BOOST_LOG_SEV(d->m_logger, log::error) << "Master rejected our registration, aborting.";
        context().stop();
      } else {
        BOOST_LOG_SEV(d->m_logger, log::normal) << "Successfully registered on the master node.";

        // Reset the master missed heartbeats counter
        d->m_masterMissedHeartbeats = 0;

        // Start sending heartbeats as master will now expect them
        heartbeat();
      }
    },
    [this](RpcErrorCode code, const std::string &msg) {
      if (code == RpcErrorCode::RequestTimedOut) {
        // Retry cluster join
        BOOST_LOG_SEV(d->m_logger, log::warning) << "Join request timed out, retrying.";

        // Attempt to rejoin immediately as at least 5 seconds will have passed
        joinCluster();
      } else {
        // Some issue is preventing us from joining
        BOOST_LOG_SEV(d->m_logger, log::error) << "Master rejected our registration: " << msg;
        BOOST_LOG_SEV(d->m_logger, log::error) << "Aborting.";
        context().stop();
      }
    }
  );
}

void Slave::rejoinCluster()
{
  // TODO: Leave cluster?
  d->m_heartbeatTimer.cancel();

  joinCluster();
}

void Slave::heartbeat(const boost::system::error_code &error)
{
  if (error)
    return;

  d->m_master.call<Protocol::ClusterHeartbeat, Protocol::ClusterHeartbeat>(
    "Testbed.Cluster.Heartbeat",
    Protocol::ClusterHeartbeat(),
    nullptr,
    [this](RpcErrorCode, const std::string&) {
      // Failed to receive heartbeat from master
      if (++d->m_masterMissedHeartbeats > 2) {
        BOOST_LOG_SEV(d->m_logger, log::error) << "Connection to master has timed out!";
        rejoinCluster();
      }
    }
  );

  d->m_heartbeatTimer.expires_from_now(boost::posix_time::seconds(5));
  d->m_heartbeatTimer.async_wait(boost::bind(&Slave::heartbeat, this, _1));
}

}

}
