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
#include "testbed/cluster/node.h"
#include "testbed/exceptions.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"

#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/empty_deleter.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>

namespace po = boost::program_options;

namespace UniSphere {

namespace TestBed {

class ClusterNodePrivate {
public:
  void initialize(const NodeIdentifier &nodeId,
                  const std::string &ip,
                  unsigned short port);

  void formatLogRecord(const logging::record_view &rec, logging::formatting_ostream &stream);
public:
  /// Cluster node communication context
  Context m_context;
  /// Link manager
  boost::shared_ptr<LinkManager> m_linkManager;
  /// RPC communication channel
  boost::shared_ptr<InterplexRpcChannel> m_channel;
  /// RPC engine
  boost::shared_ptr<RpcEngine<InterplexRpcChannel>> m_rpc;
};

void ClusterNodePrivate::formatLogRecord(const logging::record_view &rec, logging::formatting_ostream &stream)
{
  stream << "[" << logging::extract<boost::posix_time::ptime>("TimeStamp", rec) << "] ";
  stream << "<" << logging::extract<log::LogSeverityLevel, LogTags::Severity>("Severity", rec) << "> ";

  auto nodeId = logging::extract<NodeIdentifier>("LocalNodeID", rec);
  if (!nodeId.empty())
    stream << "[" << nodeId.get().hex() << "] ";
  else
    stream << "[global] ";

  stream << "[" << logging::extract<std::string>("Channel", rec) << "] ";
  stream << rec[logging::expressions::smessage];
}

void ClusterNodePrivate::initialize(const NodeIdentifier &nodeId,
                                    const std::string &ip,
                                    unsigned short port)
{
  // Setup a logging sink
  auto sink = logging::add_console_log(std::clog);
  sink->set_formatter(boost::bind(&ClusterNodePrivate::formatLogRecord, this, _1, _2));
  logging::core::get()->set_logging_enabled(true);

  // Initialize the link manager
  m_linkManager = boost::shared_ptr<LinkManager>(new LinkManager(m_context, nodeId));
  m_linkManager->setLocalAddress(Address(ip, 0));
  m_linkManager->listen(Address(ip, port));

  // Initialize the RPC engine
  m_channel = boost::shared_ptr<InterplexRpcChannel>(new InterplexRpcChannel(*m_linkManager));
  m_rpc = boost::shared_ptr<RpcEngine<InterplexRpcChannel>>(new RpcEngine<InterplexRpcChannel>(*m_channel));
}

ClusterNode::ClusterNode()
  : d(new ClusterNodePrivate)
{
}

Context &ClusterNode::context()
{
  return d->m_context;
}
  
LinkManager &ClusterNode::linkManager()
{
  return *d->m_linkManager;
}

RpcEngine<InterplexRpcChannel> &ClusterNode::rpc()
{
  return *d->m_rpc;
}

void ClusterNode::setupOptions(int argc,
                               char **argv,
                               po::options_description &options,
                               po::variables_map &variables)
{
  if (variables.empty()) {
    // Local options
    po::options_description local("General Cluster Options");
    local.add_options()
      ("cluster-ip", po::value<std::string>(), "local IP address used for cluster control")
      ("cluster-port", po::value<unsigned short>()->default_value(8471), "local port used for cluster control")
      ("cluster-node-id", po::value<std::string>(), "node identifier for the local cluster node (optional)")
    ;
    options.add(local);
    return;
  }

  // Process local options
  NodeIdentifier nodeId = NodeIdentifier::random();

  // Validate options
  if (!variables.count("cluster-ip")) {
    throw ArgumentError("Missing required --cluster-ip option!");
  } else if (!variables.count("cluster-port")) {
    throw ArgumentError("Missing required --cluster-port option!");
  } else if (variables.count("cluster-node-id")) {
    nodeId = NodeIdentifier(variables["cluster-node-id"].as<std::string>(), NodeIdentifier::Format::Hex);
    if (!nodeId.isValid())
      throw ArgumentError("Invalid node identifier specified!");
  }

  // Initialize the cluster node
  d->initialize(
    nodeId,
    variables["cluster-ip"].as<std::string>(),
    variables["cluster-port"].as<unsigned short>()
  );
}

void ClusterNode::start()
{
  run();
  d->m_context.run(1);
}

}

}
