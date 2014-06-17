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
#include "testbed/cluster/topology_loader.h"
#include "testbed/exceptions.h"

#include <unordered_set>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/range/adaptors.hpp>
#include <botan/botan.h>

namespace UniSphere {

namespace TestBed {

/// Graph representation type
typedef boost::adjacency_list<
  boost::listS,
  boost::vecS,
  boost::undirectedS,
  boost::property<boost::vertex_name_t, std::string>,
  boost::property<boost::edge_weight_t, double>
> Topology;

/// Vertex descriptor type
typedef typename boost::graph_traits<Topology>::vertex_descriptor TopologyVertex;

class TopologyLoaderPrivate {
public:
  TopologyLoaderPrivate();

  boost::shared_ptr<boost::dynamic_property_map> generateMap(const std::string &pmap,
                                                             const boost::any &key,
                                                             const boost::any &value);

  int assignNodeToPartition(const std::string &name,
                            const NodeIdentifier &nodeId,
                            size_t partitions);

  Contact assignContact(Partition &part, const PublicPeerKey &peerKey);

  PrivatePeerKey assignPrivateKey(const std::string &name);
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
  /// Mapping of nodes to contacts
  std::unordered_map<NodeIdentifier, Contact> m_contacts;
  /// Mapping of nodes to private keys
  std::unordered_map<std::string, PrivatePeerKey> m_privateKeys;
  /// Mapping of nodes to descriptors
  std::unordered_map<NodeIdentifier, Partition::Node> m_nodes;
  /// Nodes in BFS traversal order
  boost::shared_ptr<std::list<Partition::Node>> m_nodesBfs;
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

TopologyLoaderPrivate::TopologyLoaderPrivate()
  : m_topology(0),
    m_properties(boost::bind(&TopologyLoaderPrivate::generateMap, this, _1, _2, _3)),
    m_idGenType(TopologyLoader::IdGenerationType::Random)
{
}

int TopologyLoaderPrivate::assignNodeToPartition(const std::string &name,
                                                 const NodeIdentifier &nodeId,
                                                 size_t partitions)
{
  return std::hash<NodeIdentifier>()(nodeId) % partitions;
}

Contact TopologyLoaderPrivate::assignContact(Partition &part, const PublicPeerKey &peerKey)
{
  auto it = m_contacts.find(peerKey.nodeId());
  if (it != m_contacts.end())
    return it->second;

  Contact contact(peerKey);
  contact.addAddress(Address(part.ip, part.usedPorts++));
  // TODO: Handle situations when we are out of ports
  m_contacts.insert({{ peerKey.nodeId(), contact }});
  return contact;
}

PrivatePeerKey TopologyLoaderPrivate::assignPrivateKey(const std::string &name)
{
  auto it = m_privateKeys.find(name);
  if (it != m_privateKeys.end())
    return it->second;

  // TODO: This currently ignores TopologyLoader::IdGenerationType

  PrivatePeerKey key;
  key.generate();
  m_privateKeys.insert({{ name, key }});
  return key;
}

TopologyLoader::TopologyLoader()
  : d(new TopologyLoaderPrivate)
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

void TopologyLoader::partition(const SlaveDescriptorMap &slaves, IdGenerationType idGenType)
{
  Topology &topology = d->m_topology;
  std::vector<Partition> &partitions = d->m_partitions;
  d->m_idGenType = idGenType;

  // Create one partition per slave
  size_t index = 0;
  for (const SlaveDescriptor &slave : slaves | boost::adaptors::map_values) {
    partitions.push_back(Partition{ index++, slave.contact, slave.simulationIp, slave.simulationPortRange,
     std::get<0>(slave.simulationPortRange) });
  }

  // Iterate through all vertices and put them into appropriate partitions
  std::unordered_set<TopologyVertex> nodes;
  std::unordered_set<std::string> nodeNames;
  for (auto vp = boost::vertices(topology); vp.first != vp.second; ++vp.first) {
    std::string name = boost::get(boost::vertex_name, topology, *vp.first);

    // Abort when there is a vertex without a label
    if (name.empty())
      throw TopologyMalformed("One of the nodes has an empty label!");
    // Abort when there is a node with a duplicate label
    if (nodeNames.find(name) != nodeNames.end())
      throw TopologyMalformed("At least one node has a duplicate label '" + name + "'!");

    PrivatePeerKey privateKey = d->assignPrivateKey(name);
    NodeIdentifier nodeId = privateKey.nodeId();
    Partition &part = partitions[d->assignNodeToPartition(name, nodeId, partitions.size())];
    Partition::Node node{ part.index, name, d->assignContact(part, privateKey), privateKey };

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
      PrivatePeerKey privateKey = d->assignPrivateKey(peerName);
      NodeIdentifier peerId = privateKey.nodeId();
      Partition &peerPart = partitions[d->assignNodeToPartition(peerName, peerId, partitions.size())];
      Contact contact = d->assignContact(peerPart, privateKey);

      node.peers.push_back(Peer(contact));
    }

    part.nodes.push_back(node);
    nodes.insert(*vp.first);
    nodeNames.insert(name);
    d->m_nodes.insert({{ node.contact.nodeId(), node }});
  }

  // Prepare a list of nodes in BFS traversal order
  struct visitor : public boost::default_bfs_visitor {
    boost::shared_ptr<TopologyLoaderPrivate> d;
    std::unordered_set<TopologyVertex> &nodes;

    visitor(boost::shared_ptr<TopologyLoaderPrivate> d,
            std::unordered_set<TopologyVertex> &nodes)
      : d(d),
        nodes(nodes)
    {}

    void discover_vertex(TopologyVertex &vertex, const Topology &topology)
    {
      std::string name = boost::get(boost::vertex_name, topology, vertex);
      d->m_nodesBfs->push_back(d->m_nodes.at(d->assignPrivateKey(name).nodeId()));
      nodes.erase(vertex);
    }
  } vis(d, nodes);

  d->m_nodesBfs = boost::make_shared<std::list<Partition::Node>>();
  while (!nodes.empty()) {
    boost::breadth_first_search(topology, *nodes.begin(), boost::visitor(vis));
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

Partition::NodeRange TopologyLoader::getNodes(TopologyLoader::TraversalOrder traversal) const
{
  switch (traversal) {
    case TopologyLoader::TraversalOrder::BFS: {
      return *d->m_nodesBfs;
    }

    default:
    case TopologyLoader::TraversalOrder::Unordered: {
      return d->m_nodes | boost::adaptors::map_values;
    }
  }
}

const Partition::Node &TopologyLoader::getNodeById(const NodeIdentifier &nodeId) const
{
  return d->m_nodes.at(nodeId);
}

}

}
