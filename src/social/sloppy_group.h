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
#ifndef UNISPHERE_SOCIAL_SLOPPYGROUP_H
#define UNISPHERE_SOCIAL_SLOPPYGROUP_H

#include "core/globals.h"
#include "identity/node_identifier.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/optional/optional.hpp>
#include <boost/serialization/optional.hpp>

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
    struct NodeName { using kind = boost::vertex_property_tag; };
    /// Specifies the node's group
    struct NodeGroup { using kind = boost::vertex_property_tag; };
    /// Specifies the node's group prefix length
    struct NodeGroupPrefixLength { using kind = boost::vertex_property_tag; };
    /// Specifies whether the vertex is just a placeholder without data
    struct Placeholder { using kind = boost::vertex_property_tag; };
    /// Specifies whether the link is a foreign one
    struct LinkIsForeign { using kind = boost::edge_property_tag; };
    /// Specifies whether the link is a reverse one
    struct LinkIsReverse { using kind = boost::edge_property_tag; };
  };

  /// Graph definition for dumping the sloppy group topology into
  using TopologyDumpGraph = boost::labeled_graph<
    boost::adjacency_list<
      boost::hash_setS,
      boost::vecS,
      boost::bidirectionalS,
      boost::property<TopologyDumpTags::NodeName, std::string,
        boost::property<TopologyDumpTags::NodeGroup, std::string,
          boost::property<TopologyDumpTags::NodeGroupPrefixLength, int,
            boost::property<TopologyDumpTags::Placeholder, boost::optional<bool>>>>>,
      boost::property<TopologyDumpTags::LinkIsForeign, int,
        boost::property<TopologyDumpTags::LinkIsReverse, int>>
    >,
    std::string,
    boost::hash_mapS
  >;

  /**
   * A structure for reporting sloppy group statistics.
   */
  struct Statistics {
    /// Number of transmitted records
    size_t recordXmits = 0;
    /// Number of received records
    size_t recordRcvd = 0;
  };

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
   * Retrieves various statistics about sloppy group manager operation.
   */
  const Statistics &statistics() const;

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
