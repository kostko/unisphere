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
#include "core/context.h"
#include "interplex/link_manager.h"

namespace UniSphere {

namespace TestBed {

class ClusterNodePrivate {
public:
  ClusterNodePrivate(const NodeIdentifier &nodeId,
                     const std::string &ip,
                     unsigned short port);
public:
  /// Cluster node communication context
  Context m_context;
  /// Link manager
  LinkManager m_linkManager;
};

ClusterNodePrivate::ClusterNodePrivate(const NodeIdentifier &nodeId,
                                       const std::string &ip,
                                       unsigned short port)
  : m_linkManager(m_context, nodeId)
{
  m_linkManager.setLocalAddress(Address(ip, 0));
  m_linkManager.listen(Address(ip, port));
}

ClusterNode::ClusterNode(const NodeIdentifier &nodeId,
                         const std::string &ip,
                         unsigned short port)
  : d(new ClusterNodePrivate(nodeId, ip, port))
{
}

Context &ClusterNode::context()
{
  return d->m_context;
}
  
LinkManager &ClusterNode::linkManager()
{
  return d->m_linkManager;
}

void ClusterNode::start()
{
  // Invoke per-node-type initialization function
  initialize();

  // Start the cluster node context and block until we are done
  d->m_context.run(1);
}

}

}
