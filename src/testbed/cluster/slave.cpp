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
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/rpc_channel.h"
#include "rpc/engine.hpp"

namespace UniSphere {

namespace TestBed {

class SlavePrivate {
public:
  // TODO
};

Slave::Slave()
  : ClusterNode(),
    d(new SlavePrivate)
{
}

void Slave::run()
{
  context().logger()
    << Logger::Component{"Slave"}
    << Logger::Level::Info
    << "Cluster slave initialized." << std::endl;

  // Start announcing ourselves to master node
}

}

}
