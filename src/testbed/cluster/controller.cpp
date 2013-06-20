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
#include "identity/node_identifier.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"
#include "rpc/service.hpp"
#include "src/testbed/cluster/messages.pb.h"

#include <unordered_map>
#include <sstream>
#include <boost/range/adaptors.hpp>

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

class ControllerPrivate {
public:
  ControllerPrivate();
public:
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
};

ControllerPrivate::ControllerPrivate()
  : m_logger(logging::keywords::channel = "cluster_controller")
{
}

Controller::Controller()
  : ClusterNode(),
    d(new ControllerPrivate)
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

  scenario->initialize(argc, argv, options);
}

void Controller::run()
{
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

      int i = 0;
      BOOST_LOG_SEV(d->m_logger, log::normal) << "Loaded topology with " << loader.getTopologySize() << " nodes.";
      for (const TopologyLoader::Partition &part : partitions) {
        BOOST_LOG_SEV(d->m_logger, log::normal) << "  [] Partition " << ++i << ": " << part.nodes.size() << " nodes";
      }

      // TODO: Instruct each slave to create its own partition
    },
    [this](RpcErrorCode code, const std::string &msg) {
      BOOST_LOG_SEV(d->m_logger, log::error) << "Failed to start simulation: " << msg;
      context().stop();
    }
  );
}

}

}
