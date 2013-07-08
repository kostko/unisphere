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
#ifndef UNISPHERE_TESTBED_DATASETGRAPHS_H
#define UNISPHERE_TESTBED_DATASETGRAPHS_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/graphml.hpp>

#include <fstream>

namespace UniSphere {

namespace TestBed {

namespace detail {

template <class LabeledGraph, class NameAttributeTag>
void mergeGraph(const typename LabeledGraph::graph_type &g, LabeledGraph &result)
{
  typedef typename LabeledGraph::graph_type Graph;

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

    // Copy all vertex properties
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

template <class LabeledGraph, class NameAttributeTag, class Dataset>
void mergeGraphDataset(const Dataset &dataset, const std::string &key, LabeledGraph &result)
{
  typedef typename LabeledGraph::graph_type Graph;

  for (const auto &record : dataset) {
    const Graph &g = boost::get<Graph>(record.at(key));
    detail::mergeGraph<LabeledGraph, NameAttributeTag>(g, result);
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

}

}

#endif
