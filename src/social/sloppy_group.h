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
#ifndef UNISPHERE_SOCIAL_SLOPPYGROUP_H
#define UNISPHERE_SOCIAL_SLOPPYGROUP_H

#include "core/globals.h"
#include "identity/node_identifier.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>

namespace UniSphere {

class CompactRouter;
class NetworkSizeEstimator;

/**
 * Represents the sloppy group overlay manager.
 */
class UNISPHERE_EXPORT SloppyGroupManager {
public:
  /// Tags for topology dump graph properties.
  struct TopologyDumpTags {
    /// Specifies the node's name
    struct NodeName { typedef boost::vertex_property_tag kind; };
    /// Specifies the node's group
    struct NodeGroup { typedef boost::vertex_property_tag kind; };
    /// Specifies whether the link is a foreign one
    struct LinkIsForeign { typedef boost::edge_property_tag kind; };
    /// Specifies whether the link is a reverse one
    struct LinkIsReverse { typedef boost::edge_property_tag kind; };
  };

  /// Graph definition for dumping the sloppy group topology into
  typedef boost::labeled_graph<
    boost::adjacency_list<
      boost::vecS,
      boost::vecS,
      boost::bidirectionalS,
      boost::property<TopologyDumpTags::NodeName, std::string,
        boost::property<TopologyDumpTags::NodeGroup, std::string>>,
      boost::property<TopologyDumpTags::LinkIsForeign, int,
        boost::property<TopologyDumpTags::LinkIsReverse, int>>
    >,
    std::string,
    boost::hash_mapS
  > TopologyDumpGraph;

  /**
   * Class constructor.
   *
   * @param router Router instance
   * @param sizeEstimator Size estimator instance
   */
  SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  SloppyGroupManager(const SloppyGroupManager&) = delete;
  SloppyGroupManager &operator=(const SloppyGroupManager&) = delete;

  /**
   * Initializes the sloppy group manager component.
   */
  void initialize();

  /**
   * Shuts down the sloppy group manager component.
   */
  void shutdown();

  /**
   * Returns the current group prefix length.
   */
  size_t getGroupPrefixLength() const;

  /**
   * Returns the current group prefix.
   */
  const NodeIdentifier &getGroupPrefix() const;

  /**
   * Returns the combined size of all peer views.
   */
  size_t sizePeerViews() const;

  /**
   * Outputs the sloppy group state to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream,
            std::function<std::string(const NodeIdentifier&)> resolve = nullptr);

  /**
   * Dumps the locally known sloppy group topology into a graph.
   *
   * @param graph Output graph to dump into
   * @param resolve Optional name resolver
   */
  void dumpTopology(TopologyDumpGraph &graph,
                    std::function<std::string(const NodeIdentifier&)> resolve = nullptr);
private:
  UNISPHERE_DECLARE_PRIVATE(SloppyGroupManager)
};

}

#endif
