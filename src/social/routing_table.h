/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#ifndef UNISPHERE_SOCIAL_ROUTINGTABLE_H
#define UNISPHERE_SOCIAL_ROUTINGTABLE_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/signal.hpp>

#include "core/context.h"
#include "identity/node_identifier.h"
#include "social/size_estimator.h"

namespace midx = boost::multi_index;

namespace UniSphere {

/// The routing path type that contains a list of vports to reach a destination
typedef std::vector<std::uint32_t> RoutingPath;

class UNISPHERE_EXPORT RoutingEntry {
public:
  enum class Type : std::uint8_t {
    Null        = 0x00,
    Vicinity    = 0x01,
    Direct      = 0x02,
    Landmark    = 0x03,
  };
  
  RoutingEntry();

  bool isNull() const { return destination.isNull() || type == Type::Null; }

  bool operator==(const RoutingEntry &other) const;
public:
  /// Destination node identifier
  NodeIdentifier destination;
  /// Path of vports to destination
  RoutingPath path;
  /// Entry type
  Type type;
  /// Cost to route to that entry
  std::uint32_t cost;
};

/// RIB index tags
namespace RIBTags {
  class DestinationId;
  class TypeCost;
}

typedef boost::multi_index_container<
  RoutingEntry,
  midx::indexed_by<
    // Index by destination identifier and order by cost within
    midx::ordered_non_unique<
      midx::tag<RIBTags::DestinationId>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, std::uint32_t, cost)
      >
    >,
    
    // Index by type and cost
    midx::ordered_non_unique<
      midx::tag<RIBTags::TypeCost>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, RoutingEntry::Type, type),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, std::uint32_t, cost)
      >
    >
  >
> RoutingInformationBase;

class LandmarkAddress {
public:
  LandmarkAddress(const NodeIdentifier &landmarkId, const RoutingPath &path);
public:
  NodeIdentifier m_landmarkId;
  RoutingPath m_path;
};

class UNISPHERE_EXPORT CompactRoutingTable {
public:
  /**
   * Class constructor.
   *
   * @param sizeEstimator A network size estimator
   */
  CompactRoutingTable(NetworkSizeEstimator &sizeEstimator);
  
  /**
   * Attempts to import a routing entry into the routing table.
   * 
   * @param entry Routing entry to import
   * @return True if routing table has been changed, false otherwise
   */
  bool import(const RoutingEntry &entry);

  /**
   * Sets the landmark status of the local node.
   *
   * @param landmark True for the local node to become a landmark
   */
  void setLandmark(bool landmark);

  /**
   * Returns true if the local node is currently a landmark for other
   * nodes.
   */
  bool isLandmark() const { return m_landmark; }
public:
  /// Signal gets called when a routing entry should be exported to neighbours
  boost::signal<void(const RoutingEntry&)> signalExportEntry;
protected:
  size_t getMaximumVicinitySize() const;
private:
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Mutex protecting the routing table
  std::recursive_mutex m_mutex;
  /// Information needed for routing to any node
  RoutingInformationBase m_rib;
  /// Local address based on nearest landmark; multiple for redundancy?
  std::list<LandmarkAddress> m_localAddress;
  /// Landmark status of the current node
  bool m_landmark;
};
  
}

#endif
