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
  /// Master contact
  Contact m_masterContact;
  /// Heartbeat timer
  boost::asio::deadline_timer m_heartbeatTimer;
};

SlavePrivate::SlavePrivate(Context &context)
  : m_heartbeatTimer(context.service())
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

    // Testbed options on master node
    po::options_description local("Slave Cluster Options");
    local.add_options()
      ("cluster-master-ip", po::value<std::string>(), "IP address of cluster master")
      ("cluster-master-port", po::value<unsigned short>()->default_value(8471), "port of cluster master")
      ("cluster-master-id", po::value<std::string>(), "node identifier of cluster master")
    ;
    options.add(local);
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
}

void Slave::run()
{
  context().logger()
    << Logger::Component{"Slave"}
    << Logger::Level::Info
    << "Cluster slave initialized." << std::endl;

  heartbeat();
}

void Slave::heartbeat()
{
  rpc().call<Protocol::ClusterHeartbeat>(
    d->m_masterContact.nodeId(),
    "Testbed.Cluster.Heartbeat",
    Protocol::ClusterHeartbeat(),
    rpc().options().setChannelOptions(MessageOptions().setContact(d->m_masterContact))
  );

  d->m_heartbeatTimer.expires_from_now(boost::posix_time::seconds(15));
  d->m_heartbeatTimer.async_wait(boost::bind(&Slave::heartbeat, this));
}

}

}
