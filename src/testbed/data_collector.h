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
#ifndef UNISPHERE_TESTBED_DATACOLLECTOR_H
#define UNISPHERE_TESTBED_DATACOLLECTOR_H

#include "core/globals.h"

#include <ostream>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/graph/graph_concepts.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Data collector class can be used to output columnar data from the
 * test cases.
 */
class UNISPHERE_EXPORT DataCollector {
public:
  /**
   * A convenience structure for writing graphs into the data collector
   * stream.
   */
  template<class GraphType>
  struct Graph {
    BOOST_CONCEPT_ASSERT(( boost::VertexListGraphConcept<GraphType> ));

    /// Graph to be stored
    GraphType &graph;
    /// Dynamic properties of the graph
    boost::dynamic_properties &properties;
  };

  template<typename T>
  friend DataCollector &operator<<(DataCollector &dc, const T &t);

  friend DataCollector &operator<<(DataCollector &dc, const std::string &str);

  template<class GraphType>
  friend DataCollector &operator<<(DataCollector &dc, const DataCollector::Graph<GraphType> &g);

  /**
   * Constructs a new data collector.
   *
   * @param directory Output directory
   * @param component Component name
   * @param columns A list of data column names
   * @param type Optional data type
   */
  DataCollector(const std::string &directory,
                const std::string &component,
                std::initializer_list<std::string> columns,
                const std::string &type = "csv");
protected:
  /**
   * Returns the output stream.
   */
  std::ostream &stream();

  /**
   * Advances to the next column in set.
   */
  void nextColumn();
private:
  UNISPHERE_DECLARE_PRIVATE(DataCollector)
};

/**
 * Serialization operator for string formatting.
 */
UNISPHERE_EXPORT DataCollector &operator<<(DataCollector &dc, const std::string &str);

/**
 * Serialization operator for graphs.
 */
template<class GraphType>
UNISPHERE_EXPORT DataCollector &operator<<(DataCollector &dc, const DataCollector::Graph<GraphType> &g)
{
  boost::write_graphml(dc.stream(), g.graph, g.properties);
  return dc;
}

/**
 * Catch-all serialization operator that simply redirects everything to the
 * underlying stream.
 */
template<typename T>
UNISPHERE_EXPORT DataCollector &operator<<(DataCollector &dc, const T &t)
{
  dc.stream() << t;
  dc.nextColumn();
  return dc;
}

}

}

#endif
