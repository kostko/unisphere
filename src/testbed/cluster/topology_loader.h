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
#ifndef UNISPHERE_TESTBED_TOPOLOGYLOADER_H
#define UNISPHERE_TESTBED_TOPOLOGYLOADER_H

#include "testbed/cluster/slave_descriptor.h"
#include "testbed/cluster/partition.h"

#include <vector>

namespace UniSphere {

namespace TestBed {

/**
 * Topology loader performs loading of GraphML-formatted topologies and
 * slices them into partitions that are assigned to individual slaves.
 */
class UNISPHERE_EXPORT TopologyLoader {
public:
  /**
   * Ways that node identifiers can be generated when initializing the
   * virtual nodes.
   */
  enum class IdGenerationType {
    /// Randomly assign identifiers to nodes
    Random,
    /// Generate identifiers by hashing the node names
    Consistent
  };

  /**
   * Class constructor.
   *
   * @param idGenType Type of identifier generation
   */
  TopologyLoader(IdGenerationType idGenType);

  /**
   * Loads topology from a GraphML file.
   *
   * @param filename Topology filename
   */
  void load(const std::string &filename);

  /**
   * Partitions the topology into multiple parts, one for each slave.
   *
   * @param slaves A map of slaves
   */
  void partition(const SlaveDescriptorMap &slaves);

  /**
   * Returns the number of vertices in the loaded topology.
   */
  size_t getTopologySize() const;

  /**
   * Returns the generated partitions.
   */
  const std::vector<Partition> &getPartitions() const;
private:
  UNISPHERE_DECLARE_PRIVATE(TopologyLoader)
};

}

}

#endif
