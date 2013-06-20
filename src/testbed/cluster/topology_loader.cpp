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
#include "testbed/cluster/topology_loader.h"
#include "testbed/exceptions.h"

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/range/adaptors.hpp>
#include <botan/botan.h>

namespace UniSphere {

namespace TestBed {

/// Graph representation type
typedef boost::adjacency_list<
  boost::listS,
  boost::listS,
  boost::undirectedS,
  boost::property<boost::vertex_name_t, std::string>,
  boost::property<boost::edge_weight_t, double>
> Topology;

class TopologyLoaderPrivate {
public:
  TopologyLoaderPrivate(TopologyLoader::IdGenerationType idGenType);

  NodeIdentifier getNodeId(const std::string &name);

  int assignNodeToPartition(const std::string &name,
                            const NodeIdentifier &nodeId,
                            size_t partitions);

  Contact assignContact(TopologyLoader::Partition &part, const NodeIdentifier &nodeId);
public:
  /// Graph
  Topology m_topology;
  /// Generated partitions
  std::vector<TopologyLoader::Partition> m_partitions;
  /// Identifier generation type
  TopologyLoader::IdGenerationType m_idGenType;
  /// Mapping of node names to identifiers
  std::unordered_map<std::string, NodeIdentifier> m_names;
  /// Mapping of nodes to contacts
  std::unordered_map<NodeIdentifier, Contact> m_contacts;
};

TopologyLoaderPrivate::TopologyLoaderPrivate(TopologyLoader::IdGenerationType idGenType)
  : m_topology(0),
    m_idGenType(idGenType)
{
}

NodeIdentifier TopologyLoaderPrivate::getNodeId(const std::string &name)
{
  auto it = m_names.find(name);
  if (it != m_names.end())
    return it->second;

  // Generate new identifier accoording to the selected function
  NodeIdentifier nodeId;
  switch (m_idGenType) {
    case TopologyLoader::IdGenerationType::Consistent: {
      Botan::Pipe pipe(new Botan::Hash_Filter("SHA-1"));
      pipe.process_msg(name);
      nodeId = NodeIdentifier(pipe.read_all_as_string(0), NodeIdentifier::Format::Raw);
    }
    default:
    case TopologyLoader::IdGenerationType::Random: {
      nodeId = NodeIdentifier::random();
    }
  }
  m_names.insert({{ name, nodeId }});

  return nodeId;
}

int TopologyLoaderPrivate::assignNodeToPartition(const std::string &name,
                                                 const NodeIdentifier &nodeId,
                                                 size_t partitions)
{
  return std::hash<NodeIdentifier>()(nodeId) % partitions;
}

Contact TopologyLoaderPrivate::assignContact(TopologyLoader::Partition &part, const NodeIdentifier &nodeId)
{
  auto it = m_contacts.find(nodeId);
  if (it != m_contacts.end())
    return it->second;

  Contact contact(nodeId);
  contact.addAddress(Address(part.ip, part.usedPorts++));
  // TODO: Handle situations when we are out of ports
  m_contacts.insert({{ nodeId, contact }});
  return contact;
}

TopologyLoader::TopologyLoader(IdGenerationType idGenType)
  : d(new TopologyLoaderPrivate(idGenType))
{
}

void TopologyLoader::load(const std::string &filename)
{
  Topology &topology = d->m_topology;
  boost::dynamic_properties properties;
  properties.property("label", boost::get(boost::vertex_name, topology));
  properties.property("weight", boost::get(boost::edge_weight, topology));

  // Open the topology data file and try to parse GraphML data
  std::ifstream file(filename);
  if (!file)
    throw TopologyLoadingFailed(filename);

  try {
    boost::read_graphml(file, topology, properties);
  } catch (std::exception &e) {
    throw TopologyLoadingFailed(filename);
  }
}

void TopologyLoader::partition(const SlaveDescriptorMap &slaves)
{
  Topology &topology = d->m_topology;
  std::vector<Partition> &partitions = d->m_partitions;

  // Create one partition per slave
  for (const SlaveDescriptor &slave : slaves | boost::adaptors::map_values) {
    partitions.push_back(Partition{ slave.contact, slave.simulationIp, slave.simulationPortRange,
     std::get<0>(slave.simulationPortRange) });
  }

  // Iterate through all vertices and put them into appropriate partitions
  for (auto vp = boost::vertices(topology); vp.first != vp.second; ++vp.first) {
    std::string name = boost::get(boost::vertex_name, topology, *vp.first);
    NodeIdentifier nodeId = d->getNodeId(name);
    Partition &part = partitions[d->assignNodeToPartition(name, nodeId, partitions.size())];
    Partition::Node node{ name, d->assignContact(part, nodeId) };

    // Add peers
    for (auto np = boost::adjacent_vertices(*vp.first, topology); np.first != np.second; ++np.first) {
      std::string peerName = boost::get(boost::vertex_name, topology, *np.first);
      NodeIdentifier peerId = d->getNodeId(peerName);
      Partition &peerPart = partitions[d->assignNodeToPartition(peerName, peerId, partitions.size())];

      node.peers.push_back(d->assignContact(peerPart, peerId));
    }

    part.nodes.push_back(node);
  }
}

size_t TopologyLoader::getTopologySize() const
{
  return boost::num_vertices(d->m_topology);
}

const std::vector<TopologyLoader::Partition> &TopologyLoader::getPartitions() const
{
  return d->m_partitions;
}

}

}
