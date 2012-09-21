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

class UNISPHERE_EXPORT RoutingPath {
public:
  RoutingPath();
  
  explicit RoutingPath(const std::string &path);
  
  inline bool isNull() const { return m_path.empty(); }
  
  size_t size() const;
  
  std::uint32_t operator[](size_t index) const;
  
  RoutingPath reversed() const;
private:
  /// Raw encoded representation of a path
  std::string m_path;
};

class UNISPHERE_EXPORT RoutingEntry {
public:
  enum class Type : std::uint8_t {
    Vicinity    = 0x00,
    Direct      = 0x01,
    Landmark    = 0x02,
  };
  
  RoutingEntry();
  
  size_t getPathLength() const { return path.size(); }
public:
  /// Destination node identifier
  NodeIdentifier destination;
  /// Path of vports to destination
  RoutingPath path;
  /// Entry type
  Type type;
};

/// RIB index tags
namespace RIBTags {
  class DestinationId;
  class PathLength;
}

typedef boost::multi_index_container<
  RoutingEntry,
  midx::indexed_by<
    // Index by destination identifier and order by path length within
    midx::ordered_non_unique<
      midx::tag<RIBTags::DestinationId>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        midx::const_mem_fun<RoutingEntry, size_t, &RoutingEntry::getPathLength>
      >
    >,
    
    // Index by path length
    midx::ordered_non_unique<
      midx::tag<RIBTags::PathLength>,
      midx::const_mem_fun<RoutingEntry, size_t, &RoutingEntry::getPathLength>
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
  CompactRoutingTable(NetworkSizeEstimator &sizeEstimator);
  
  /**
   * Attempts to import a routing entry into the routing table.
   * 
   * @param entry Routing entry to import
   * @return True if routing table has been changed, false otherwise
   */
  bool import(const RoutingEntry &entry);
public:
  /// Signal gets called when a routing entry should be exported to neighbours
  boost::signal<void(const RoutingEntry&)> signalExportEntry;
private:
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Information needed for routing to any node
  RoutingInformationBase m_rib;
  /// Local address based on nearest landmark; multiple for redundancy?
  std::list<LandmarkAddress> m_localAddress;
};
  
}

#endif
