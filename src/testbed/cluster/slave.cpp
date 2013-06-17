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
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"
#include "src/testbed/cluster/messages.pb.h"

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

class SlavePrivate {
public:
  SlavePrivate(Context &context);
public:
  /// Logger instance
  Logger m_logger;
  /// Master contact
  Contact m_masterContact;
  /// Simulation IP address
  std::string m_simulationIp;
  /// Simulation port range
  std::tuple<unsigned short, unsigned short> m_simulationPortRange;
  /// Heartbeat timer
  boost::asio::deadline_timer m_heartbeatTimer;
};

SlavePrivate::SlavePrivate(Context &context)
  : m_logger(logging::keywords::channel = "cluster_slave"),
    m_heartbeatTimer(context.service())
{
}

Slave::Slave()
  : ClusterNode(),
    d(new SlavePrivate(context()))
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

  if (!variables.count("sim-ip")) {
    throw ArgumentError("Missing required --sim-ip option!");
  } else if (!variables.count("sim-port-start")) {
    throw ArgumentError("Missing required --sim-port-start option!");
  } else if (!variables.count("sim-port-end")) {
    throw ArgumentError("Missing required --sim-port-end option!");
  }

  d->m_simulationIp = variables["sim-ip"].as<std::string>();
  d->m_simulationPortRange = std::make_tuple(
    variables["sim-port-start"].as<unsigned short>(),
    variables["sim-port-end"].as<unsigned short>()
  );

  if (std::get<0>(d->m_simulationPortRange) > std::get<1>(d->m_simulationPortRange)) {
    throw ArgumentError("Invalid port range specified!");
  }
}

void Slave::run()
{
  BOOST_LOG_SEV(d->m_logger, log::normal) << "Cluster slave initialized.";

  joinCluster();
}

void Slave::joinCluster()
{
  Protocol::ClusterJoinRequest request;
  request.set_simulationip(d->m_simulationIp);
  request.set_simulationportstart(std::get<0>(d->m_simulationPortRange));
  request.set_simulationportend(std::get<1>(d->m_simulationPortRange));

  rpc().call<Protocol::ClusterJoinRequest, Protocol::ClusterJoinResponse>(
    d->m_masterContact.nodeId(),
    "Testbed.Cluster.Join",
    request,
    [this](const Protocol::ClusterJoinResponse &response, const Message&) {
      // Check if registration succeeded
      if (!response.registered()) {
        BOOST_LOG_SEV(d->m_logger, log::error) << "Master rejected our registration, aborting.";
        context().stop();
      } else {
        BOOST_LOG_SEV(d->m_logger, log::normal) << "Successfully registered on the master node.";

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
        // Some issue with the master is preventing us from joining
        BOOST_LOG_SEV(d->m_logger, log::error) << "Failure communicating with the master, aborting.";
        context().stop();
      }
    },
    rpc().options()
         .setTimeout(5)
         .setChannelOptions(
            MessageOptions().setContact(d->m_masterContact)
          )
  );
}

void Slave::heartbeat()
{
  rpc().call<Protocol::ClusterHeartbeat>(
    d->m_masterContact.nodeId(),
    "Testbed.Cluster.Heartbeat",
    Protocol::ClusterHeartbeat(),
    rpc().options().setChannelOptions(
      MessageOptions().setContact(d->m_masterContact)
    )
  );

  d->m_heartbeatTimer.expires_from_now(boost::posix_time::seconds(15));
  d->m_heartbeatTimer.async_wait(boost::bind(&Slave::heartbeat, this));
}

}

}
