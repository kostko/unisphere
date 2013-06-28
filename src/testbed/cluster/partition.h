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
#ifndef UNISPHERE_TESTBED_PARTITION_H
#define UNISPHERE_TESTBED_PARTITION_H

#include "interplex/contact.h"

#include <list>

namespace UniSphere {

namespace TestBed {

/**
 * Partition is an assignment of nodes to slaves.
 */
struct Partition {
  struct Node {
    /// Node name (from original topology file)
    std::string name;
    /// Assigned contact
    Contact contact;
    /// A list of peers in the topology
    std::list<Contact> peers;
  };

  /// Slave that will own this partition
  Contact slave;
  /// IP address for nodes in this partition
  std::string ip;
  /// Port range for nodes in this partition
  std::tuple<unsigned short, unsigned short> ports;
  /// Last used port
  unsigned short usedPorts;
  /// A list of nodes assigned to this partition
  std::list<Node> nodes;
};

}

}

#endif
