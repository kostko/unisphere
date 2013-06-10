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
#include "identity/node_identifier.h"

#include <unordered_map>

namespace UniSphere {

namespace TestBed {

class SlaveDescriptor {
public:
  NodeIdentifier nodeId;
  // TODO: Assigned virtual nodes
};

class MasterPrivate {
public:
  // TODO
public:
  /// Registered slaves
  std::unordered_map<NodeIdentifier, SlaveDescriptor> m_slaves;
};

Master::Master(const NodeIdentifier &nodeId,
               const std::string &ip,
               unsigned short port)
  : ClusterNode(nodeId, ip, port)
{
}

void Master::initialize()
{
}

}

}
