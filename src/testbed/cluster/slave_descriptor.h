/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_TESTBED_SLAVEDESCRIPTOR_H
#define UNISPHERE_TESTBED_SLAVEDESCRIPTOR_H

#include "interplex/contact.h"
#include "interplex/rpc_channel.h"
#include "rpc/service.hpp"

#include <unordered_map>

namespace UniSphere {

namespace TestBed {

/**
 * Descriptor of a slave node's information.
 */
class SlaveDescriptor {
public:
  /// Slave contact information
  Contact contact;
  /// IP address available for simulation
  std::string simulationIp;
  /// Port range available for simulation
  std::tuple<unsigned short, unsigned short> simulationPortRange;
  /// Service for communicating with the slave
  RpcService<InterplexRpcChannel> service;
};

/// A mapping of slave descriptors
typedef std::unordered_map<NodeIdentifier, SlaveDescriptor> SlaveDescriptorMap;

}

}

#endif
