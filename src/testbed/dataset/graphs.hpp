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
#ifndef UNISPHERE_TESTBED_DATASETGRAPHS_H
#define UNISPHERE_TESTBED_DATASETGRAPHS_H

#include "testbed/dataset/dataset.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/graphml.hpp>

#include <fstream>

namespace UniSphere {

namespace TestBed {

namespace detail {

/**
 * Property map for all properties encountered in GraphML topology files.
 */
template <class Vertex>
class AnyPropertyMap : public boost::dynamic_property_map
{
public:
  AnyPropertyMap(const boost::any &valueType)
    : m_type(valueType.type())
  {
  }

  boost::any get(const boost::any &key)
  {
    auto it = m_map.find(boost::any_cast<Vertex>(key));
    if (it == m_map.end())
      return boost::any();

    return it->second;
  }

  std::string get_string(const boost::any &key)
  {
    boost::any value = get(key);
    if (value.empty())
      return std::string();
    else if (m_type == typeid(std::string))
      return boost::any_cast<std::string>(value);
    else if (m_type == typeid(int))
      return boost::lexical_cast<std::string>(boost::any_cast<int>(value));
    else if (m_type == typeid(unsigned int))
      return boost::lexical_cast<std::string>(boost::any_cast<unsigned int>(value));
    else if (m_type == typeid(long))
      return boost::lexical_cast<std::string>(boost::any_cast<long>(value));
    else if (m_type == typeid(float))
      return boost::lexical_cast<std::string>(boost::any_cast<float>(value));
    else if (m_type == typeid(double))
      return boost::lexical_cast<std::string>(boost::any_cast<double>(value));
    else if (m_type == typeid(bool))
      return boost::lexical_cast<std::string>(boost::any_cast<bool>(value) ? 1 : 0);
    else
      return std::string();
  }

  void put(const boost::any &key, const boost::any &value)
  {
    BOOST_ASSERT(m_type == value.type());
    m_map.insert({ boost::any_cast<Vertex>(key), value });
  }

  const std::type_info& key() const
  {
    return typeid(Vertex);
  }

  const std::type_info& value() const
  {
    return m_type;
  }
private:
  /// Actual map storage
  std::map<Vertex, boost::any> m_map;
  /// Types that are stored in the boost::any container
  const std::type_info &m_type;
};

template <class LabeledGraph, class NameAttributeTag, class PlaceholderAttributeTag>
void mergeGraph(const typename LabeledGraph::graph_type &g, LabeledGraph &result)
{
  using Graph = typename LabeledGraph::graph_type;

  typename boost::property_map<Graph, boost::vertex_all_t>::const_type vertexSourceMap =
    boost::get(boost::vertex_all, g);
  typename boost::property_map<Graph, boost::vertex_all_t>::type vertexDestMap =
    boost::get(boost::vertex_all, result.graph());
  typename boost::property_map<Graph, boost::edge_all_t>::const_type edgeSourceMap =
    boost::get(boost::edge_all, g);
  typename boost::property_map<Graph, boost::edge_all_t>::type edgeDestMap =
    boost::get(boost::edge_all, result.graph());

  // First merge all vertices
  for (auto vp = boost::vertices(g); vp.first != vp.second; ++vp.first) {
    std::string name = boost::get(NameAttributeTag(), g, *vp.first);
    auto newVertex = result.add_vertex(name);

    // Copy all vertex properties if the source vertex is not a placeholder
    boost::optional<bool> placeholder = boost::get(PlaceholderAttributeTag(), g, *vp.first);
    if (placeholder && *placeholder)
      boost::put(boost::get(NameAttributeTag(), result.graph()), newVertex, name);
    else
      boost::put(vertexDestMap, newVertex, boost::get(vertexSourceMap, *vp.first));
  }

  // Then merge all edges
  for (auto vp = boost::vertices(g); vp.first != vp.second; ++vp.first) {
    std::string srcName = boost::get(NameAttributeTag(), g, *vp.first);

    for (auto ep = boost::out_edges(*vp.first, g); ep.first != ep.second; ++ep.first) {
      std::string dstName = boost::get(NameAttributeTag(), g, boost::target(*ep.first, g));
      auto newEdge = boost::add_edge_by_label(srcName, dstName, result).first;

      // Copy all edge properties
      boost::put(edgeDestMap, newEdge, boost::get(edgeSourceMap, *ep.first));
    }
  }
}

}

template <class LabeledGraph, class NameAttributeTag, class PlaceholderAttributeTag>
void mergeGraphDataset(const DataSet &dataset, const std::string &key, LabeledGraph &result)
{
  using Graph = typename LabeledGraph::graph_type;

  for (const auto &record : dataset) {
    const Graph &g = record.field<Graph>(key);
    detail::mergeGraph<LabeledGraph, NameAttributeTag, PlaceholderAttributeTag>(g, result);
  }
}

template <class LabeledGraph>
void outputGraphDataset(const LabeledGraph &graph,
                        boost::dynamic_properties &properties,
                        const std::string &outputFilename)
{
  std::ofstream file;
  file.open(outputFilename);
  boost::write_graphml(file, graph.graph(), properties);
}

template <typename LabeledGraph, typename NameAttributeTag>
void mergeInputNodeMetadata(TestCaseApi &api,
                            LabeledGraph &graph,
                            boost::dynamic_properties &properties)
{
  using Graph = typename LabeledGraph::graph_type;
  using Vertex = typename boost::graph_traits<Graph>::vertex_descriptor;

  std::unordered_map<std::string, boost::shared_ptr<detail::AnyPropertyMap<Vertex>>> maps;

  for (auto vp = boost::vertices(graph.graph()); vp.first != vp.second; ++vp.first) {
    std::string nodeId(boost::get(NameAttributeTag(), graph.graph(), *vp.first));
    const Partition::Node &node = api.getNodeById(NodeIdentifier(nodeId, NodeIdentifier::Format::Hex));
    for (const auto &p : node.properties) {
      auto &map = maps[p.first];
      if (!map)
        map = boost::make_shared<detail::AnyPropertyMap<Vertex>>(p.second);

      map->put(*vp.first, p.second);
    }
  }

  for (auto &p : maps) {
    // Do not overwrite existing properties
    auto it = properties.lower_bound(p.first);
    if (it != properties.end() && it->first == p.first)
      continue;

    properties.insert(p.first, p.second);
  }
}

}

}

#endif
