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
#include "testbed/cluster/master.h"
#include "testbed/cluster/slave_descriptor.h"
#include "testbed/test_bed.h"
#include "testbed/exceptions.h"
#include "identity/node_identifier.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"
#include "src/testbed/cluster/messages.pb.h"

#include <sstream>
#include <boost/range/adaptors.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

template <typename ResponseType>
using Response = RpcResponse<InterplexRpcChannel, ResponseType>;

class MasterPrivate {
public:
  MasterPrivate(Master &master);

  Response<Protocol::ClusterJoinResponse> rpcClusterJoin(const Protocol::ClusterJoinRequest &request,
                                                         const Message &msg,
                                                         RpcId rpcId);

  Response<Protocol::ClusterHeartbeat> rpcClusterHeartbeat(const Protocol::ClusterHeartbeat &request,
                                                           const Message &msg,
                                                           RpcId rpcId);

  Response<Protocol::StartResponse> rpcStart(const Protocol::StartRequest &request,
                                             const Message &msg,
                                             RpcId rpcId);

  Response<Protocol::AbortResponse> rpcAbort(const Protocol::AbortRequest &request,
                                             const Message &msg,
                                             RpcId rpcId);
public:
  UNISPHERE_DECLARE_PUBLIC(Master)

  /// Mutex to protect the data structure
  std::recursive_mutex m_mutex;
  /// Logger instance
  Logger m_logger;
  /// Current cluster state
  Master::State m_state;
  /// Registered slaves
  SlaveDescriptorMap m_slaves;
  /// Slaves pending abortion
  size_t m_slavesPendingAbortion;
};

MasterPrivate::MasterPrivate(Master &master)
  : q(master),
    m_logger(logging::keywords::channel = "cluster_master"),
    m_state(Master::State::Idle)
{
}

Master::Master()
  : ClusterNode(),
    d(new MasterPrivate(*this))
{
}

Response<Protocol::ClusterJoinResponse> MasterPrivate::rpcClusterJoin(const Protocol::ClusterJoinRequest &request,
                                                                      const Message &msg,
                                                                      RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);
  Protocol::ClusterJoinResponse response;

  if (m_state == Master::State::Idle) {
    response.set_registered(true);

    if (m_slaves.find(msg.originator()) == m_slaves.end()) {
      // Perform registration
      SlaveDescriptor descriptor;
      descriptor.contact = q.linkManager().getLinkContact(msg.originator());
      descriptor.simulationIp = request.simulation_ip();
      descriptor.simulationPortRange = std::make_tuple(
        request.simulation_port_start(),
        request.simulation_port_end()
      );
      descriptor.service = q.rpc().service(
        descriptor.contact.nodeId(),
        q.rpc().options()
               .setTimeout(5)
               .setChannelOptions(
                 MessageOptions().setContact(descriptor.contact)
               )
      );

      // Perform simple validation of the port range
      if (std::get<0>(descriptor.simulationPortRange) > std::get<1>(descriptor.simulationPortRange))
        throw RpcException(RpcErrorCode::BadRequest, "Invalid simulation port range specified!");

      // Scan existing slaves and check for conflicting addresses/port ranges
      for (const SlaveDescriptor &slave : m_slaves | boost::adaptors::map_values) {
        unsigned short c0, c1, s0, s1;
        std::tie(c0, c1) = descriptor.simulationPortRange;
        std::tie(s0, s1) = slave.simulationPortRange;

        if (slave.simulationIp == descriptor.simulationIp &&
            (
              (c0 >= s0 && c0 <= s1) ||
              (c1 >= s0 && c1 <= s1) ||
              (s0 >= c0 && s0 <= c1) ||
              (s1 >= c0 && s1 <= c1)
            ))
          throw RpcException(RpcErrorCode::BadRequest, "Simulation port range overlaps with another slave!");
      }

      m_slaves.insert({{ msg.originator(), descriptor }});

      BOOST_LOG_SEV(m_logger, log::normal) << "Registered new slave (id=" << msg.originator().hex() << ").";
    }
  } else {
    // After the simulation has started new slaves can't be registered
    BOOST_LOG_SEV(m_logger, log::warning) << "Refusing registration of new slave (id=" << msg.originator().hex() << ") while simulation is running!";
    throw RpcException(RpcErrorCode::BadRequest, "Registrations are already closed!");
  }

  return response;
}

Response<Protocol::ClusterHeartbeat> MasterPrivate::rpcClusterHeartbeat(const Protocol::ClusterHeartbeat &request,
                                                                        const Message &msg,
                                                                        RpcId rpcId)
{
  auto it = m_slaves.find(msg.originator());
  if (it == m_slaves.end())
    throw RpcException(RpcErrorCode::BadRequest, "Slave is not registered.");

  SlaveDescriptor &descriptor = it->second;
  // TODO

  return Protocol::ClusterHeartbeat();
}

Response<Protocol::StartResponse> MasterPrivate::rpcStart(const Protocol::StartRequest &request,
                                                          const Message &msg,
                                                          RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);
  Protocol::StartResponse response;
  if (m_state == Master::State::Idle) {
    if (m_slaves.empty())
      throw RpcException(RpcErrorCode::BadRequest, "No slaves registered.");

    // Prepare response list
    for (SlaveDescriptor &slave : m_slaves | boost::adaptors::map_values) {
      Protocol::StartResponse::Slave *s = response.add_slaves();
      *s->mutable_contact() = slave.contact.toMessage();
      s->set_ip(slave.simulationIp);
      s->set_port_start(std::get<0>(slave.simulationPortRange));
      s->set_port_end(std::get<0>(slave.simulationPortRange));
    }

    // Switch to running state to block new registrations
    m_state = Master::State::Running;
    BOOST_LOG_SEV(m_logger, log::normal) << "Entered 'Running' state as requested by controller.";
  } else {
    BOOST_LOG_SEV(m_logger, log::warning) << "Refusing to start after simulation has already started!";
    throw RpcException(RpcErrorCode::BadRequest, "Simulation has already started!");
  }

  return response;
}

Response<Protocol::AbortResponse> MasterPrivate::rpcAbort(const Protocol::AbortRequest &request,
                                                          const Message &msg,
                                                          RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);
  if (m_state != Master::State::Running)
    throw RpcException(RpcErrorCode::BadRequest, "Simulation is not running.");

  // Request all slaves to abort the simulation
  m_state = Master::State::Aborting;
  m_slavesPendingAbortion = m_slaves.size();
  BOOST_LOG_SEV(m_logger, log::warning) << "Entering 'Aborting' state as requested by controller.";

  auto group = q.rpc().group([this]() {
    if (m_slavesPendingAbortion != 0) {
      // When abort on some slaves has failed, we shut down
      BOOST_LOG_SEV(m_logger, log::error) << "Failed to abort on all slaves, shutting down.";
      q.context().stop();
      return;
    }

    // After successful aborts, we change into Idle state and request all slaves to
    // register again
    RecursiveUniqueLock lock(m_mutex);
    m_state = Master::State::Idle;
    m_slaves.clear();

    BOOST_LOG_SEV(m_logger, log::normal) << "Entering 'Idle' state as all slaves have aborted.";
  });

  for (SlaveDescriptor &slave : m_slaves | boost::adaptors::map_values) {
    NodeIdentifier slaveId = slave.contact.nodeId();
    slave.service.call<Protocol::AbortRequest, Protocol::AbortResponse>(
      group,
      "Testbed.Cluster.Abort",
      Protocol::AbortRequest(),
      [this, slaveId](const Protocol::AbortResponse &response, const Message&) {
        BOOST_LOG_SEV(m_logger, log::normal) << "Simulation aborted on " << slaveId.hex() << ".";
        m_slavesPendingAbortion--;
      },
      [this, slaveId](RpcErrorCode code, const std::string &msg) {
        BOOST_LOG_SEV(m_logger, log::error) << "Failed to abort simulation on " << slaveId.hex() << ": " << msg;
      }
    );
  }

  return Protocol::AbortResponse();
}

void Master::run()
{
  // Register RPC methods
  rpc().registerMethod<Protocol::ClusterJoinRequest, Protocol::ClusterJoinResponse>("Testbed.Cluster.Join",
    boost::bind(&MasterPrivate::rpcClusterJoin, d, _1, _2, _3));

  rpc().registerMethod<Protocol::ClusterHeartbeat, Protocol::ClusterHeartbeat>("Testbed.Cluster.Heartbeat",
    boost::bind(&MasterPrivate::rpcClusterHeartbeat, d, _1, _2, _3));

  rpc().registerMethod<Protocol::StartRequest, Protocol::StartResponse>("Testbed.Cluster.Start",
    boost::bind(&MasterPrivate::rpcStart, d, _1, _2, _3));

  rpc().registerMethod<Protocol::AbortRequest, Protocol::AbortResponse>("Testbed.Cluster.Abort",
    boost::bind(&MasterPrivate::rpcAbort, d, _1, _2, _3));

  BOOST_LOG_SEV(d->m_logger, log::normal) << "Cluster master initialized (id=" << linkManager().getLocalNodeId().hex() << ").";
}

}

}
