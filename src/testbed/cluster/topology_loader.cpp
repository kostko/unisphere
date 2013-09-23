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

/// Vertex descriptor type
typedef typename boost::graph_traits<Topology>::vertex_descriptor TopologyVertex;

class TopologyLoaderPrivate {
public:
  TopologyLoaderPrivate(TopologyLoader::IdGenerationType idGenType);

  boost::shared_ptr<boost::dynamic_property_map> generateMap(const std::string &pmap,
                                                             const boost::any &key,
                                                             const boost::any &value);

  NodeIdentifier getNodeId(const std::string &name);

  int assignNodeToPartition(const std::string &name,
                            const NodeIdentifier &nodeId,
                            size_t partitions);

  Contact assignContact(Partition &part, const NodeIdentifier &nodeId);
public:
  /// Graph
  Topology m_topology;
  /// Topology dynamic properties
  boost::dynamic_properties m_properties;
  /// Property map storage
  std::unordered_map<std::string, std::map<TopologyVertex, boost::any>> m_mapStorage;
  /// Generated partitions
  std::vector<Partition> m_partitions;
  /// Identifier generation type
  TopologyLoader::IdGenerationType m_idGenType;
  /// Mapping of node names to identifiers
  std::unordered_map<std::string, NodeIdentifier> m_names;
  /// Mapping of nodes to contacts
  std::unordered_map<NodeIdentifier, Contact> m_contacts;
};

/**
 * Property map for all properties encountered in GraphML topology files.
 */
class TopologyPropertyMap : public boost::dynamic_property_map
{
public:
  TopologyPropertyMap(std::map<TopologyVertex, boost::any> &map)
    : m_map(map)
  {
  }

  boost::any get(const boost::any &key)
  {
    auto it = m_map.find(boost::any_cast<TopologyVertex>(key));
    if (it == m_map.end())
      return boost::any();

    return it->second;
  }

  std::string get_string(const boost::any &key)
  {
    return std::string();
  }

  void put(const boost::any &key, const boost::any &value)
  {
    m_map.insert({ boost::any_cast<TopologyVertex>(key), value });
  }

  const std::type_info& key() const
  {
    return typeid(TopologyVertex);
  }

  const std::type_info& value() const
  {
    return typeid(boost::any);
  }
private:
  /// Reference to the actual map storage
  std::map<TopologyVertex, boost::any> &m_map;
};

boost::shared_ptr<boost::dynamic_property_map> TopologyLoaderPrivate::generateMap(const std::string &pmap,
                                                                                  const boost::any &key,
                                                                                  const boost::any &value)
{
  try {
    auto v = boost::any_cast<TopologyVertex>(key);
  } catch (boost::bad_any_cast&) {
    // Ignore non-vertex properties
    return boost::shared_ptr<boost::dynamic_property_map>();
  }
  
  return boost::static_pointer_cast<boost::dynamic_property_map>(
    boost::make_shared<TopologyPropertyMap>(m_mapStorage[pmap])
  );
}

TopologyLoaderPrivate::TopologyLoaderPrivate(TopologyLoader::IdGenerationType idGenType)
  : m_topology(0),
    m_properties(boost::bind(&TopologyLoaderPrivate::generateMap, this, _1, _2, _3)),
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
      break;
    }
    default:
    case TopologyLoader::IdGenerationType::Random: {
      nodeId = NodeIdentifier::random();
      break;
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

Contact TopologyLoaderPrivate::assignContact(Partition &part, const NodeIdentifier &nodeId)
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
  boost::dynamic_properties &properties = d->m_properties;
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
  size_t index = 0;
  for (const SlaveDescriptor &slave : slaves | boost::adaptors::map_values) {
    partitions.push_back(Partition{ index++, slave.contact, slave.simulationIp, slave.simulationPortRange,
     std::get<0>(slave.simulationPortRange) });
  }

  // Iterate through all vertices and put them into appropriate partitions
  for (auto vp = boost::vertices(topology); vp.first != vp.second; ++vp.first) {
    std::string name = boost::get(boost::vertex_name, topology, *vp.first);
    NodeIdentifier nodeId = d->getNodeId(name);
    Partition &part = partitions[d->assignNodeToPartition(name, nodeId, partitions.size())];
    Partition::Node node{ name, d->assignContact(part, nodeId) };
    for (const auto &map : d->m_properties) {
      try {
        const auto &v = map.second->get(*vp.first);
        if (!v.empty())
          node.properties[map.first] = v;
      } catch (boost::bad_any_cast&) {
        // Skip non-vertex properties
        continue;
      }
    }

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

const std::vector<Partition> &TopologyLoader::getPartitions() const
{
  return d->m_partitions;
}

}

}
