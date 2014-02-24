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
#ifndef UNISPHERE_TESTBED_PARTITION_H
#define UNISPHERE_TESTBED_PARTITION_H

#include "interplex/contact.h"
#include "social/peer.h"

#include <unordered_map>
#include <list>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <boost/range/any_range.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Partition is an assignment of nodes to slaves.
 */
struct Partition {
  struct Node {
    /// Partition index
    size_t partition;
    /// Node name (from original topology file)
    std::string name;
    /// Assigned contact
    Contact contact;
    /// Assigned private key
    PrivatePeerKey privateKey;
    /// A list of peers in the topology
    std::list<Peer> peers;
    /// Node properties from the input topology
    std::unordered_map<std::string, boost::any> properties;

    /**
     * A convenience method for property retrieval.
     */
    template <typename T>
    T property(const std::string &key) const
    {
      auto it = properties.find(key);
      if (it == properties.end())
        return T();
      return boost::any_cast<T>(it->second);
    }
  };

  /// Type for specifying traversible ranges of node descriptors
  typedef boost::any_range<
    Node,
    boost::single_pass_traversal_tag,
    Node,
    std::ptrdiff_t
  > NodeRange;

  /// Partition index
  size_t index;
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

/// Type for specifying traversible ranges of partition descriptors
typedef boost::any_range<
  Partition,
  boost::random_access_traversal_tag,
  Partition,
  std::ptrdiff_t
> PartitionRange;

/**
 * A partition selected for test case run.
 */
struct SelectedPartition {
  struct Node {
    /// Node identifier
    NodeIdentifier nodeId;
    /// Arguments for test case run
    boost::property_tree::ptree args;
  };

  /// Partition index
  size_t index;
  /// A list of selected nodes
  std::list<Node> nodes;
};

}

}

#endif
