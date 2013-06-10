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

namespace UniSphere {

namespace TestBed {

Slave::Slave(const NodeIdentifier &nodeId,
             const std::string &ip,
             unsigned short port,
             const std::string &masterIp,
             unsigned short masterPort)
  : ClusterNode(nodeId, ip, port)
{
}

void Slave::initialize()
{
  // Start announcing ourselves to master node
}

}

}
