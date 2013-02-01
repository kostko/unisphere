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

#include <unordered_map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/bimap.hpp>
#include <boost/signal.hpp>
#include <boost/tuple/tuple.hpp>

#include "core/context.h"
#include "identity/node_identifier.h"
#include "social/size_estimator.h"

namespace midx = boost::multi_index;

namespace UniSphere {

class UNISPHERE_EXPORT RouteOriginator {
public:
  RouteOriginator(const NodeIdentifier &nodeId);

  /**
   * Returns the age of this route originator descriptor.
   */
  boost::posix_time::time_duration age() const;
public:
  /// Node identifier
  NodeIdentifier identifier;
  /// Sequence number
  std::uint16_t seqno;
  /// Smallest cost that was ever advertised by the local node for this originator
  std::uint16_t smallestCost;
  /// Entry liveness
  boost::posix_time::ptime lastUpdate;
};

UNISPHERE_SHARED_POINTER(RouteOriginator)

/// Vport identifier type
typedef std::uint32_t Vport;

/// The routing path type that contains a list of vports to reach a destination
typedef std::vector<Vport> RoutingPath;

class UNISPHERE_EXPORT RoutingEntry {
public:
  /// An invalid (default-constructed) routing entry
  static const RoutingEntry INVALID;

  /**
   * Valid routing entry types.
   */
  enum class Type : std::uint8_t {
    Null        = 0x00,
    Vicinity    = 0x01,
    Landmark    = 0x02,
  };

  /**
   * A routing entry extension for storing timers.
   */
  struct Timers {
    Timers(boost::asio::io_service &service);

    /// Expiration timer
    boost::asio::deadline_timer expiryTimer;
  };
  
  /**
   * Constructs an invalid routing entry.
   */
  RoutingEntry();

  /**
   * Constructs a routing entry.
   *
   * @param destination Destination node identifier
   * @param type Routing entry type
   * @param seqno Sequence number
   */
  RoutingEntry(const NodeIdentifier &destination, Type type, std::uint16_t seqno);

  /**
   * Class destructor.
   */
  ~RoutingEntry();

  /**
   * Returns true if the entry is invalid.
   */
  bool isNull() const { return destination.isNull() || type == Type::Null; }

  /**
   * Returns true if the entry is a landmark.
   */
  bool isLandmark() const { return type == Type::Landmark; }

  /**
   * Returns true if this entry represents a direct route.
   */
  bool isDirect() const { return forwardPath.size() == 1; }

  /**
   * Returns true if this is a feasible route.
   */
  bool isFeasible() const;

  /**
   * Returns the vport identifier of the first routing hop.
   */
  Vport originVport() const { return forwardPath[0]; }

  /**
   * Returns the age of this routing entry.
   */
  boost::posix_time::time_duration age() const;

  /**
   * Comparison operator.
   */
  bool operator==(const RoutingEntry &other) const;
public:
  /// Route originator descriptor
  mutable RouteOriginatorPtr originator;
  /// Destination node identifier
  NodeIdentifier destination;
  /// Path of vports to destination
  RoutingPath forwardPath;
  /// Path of vports from destination (only for landmarks)
  RoutingPath reversePath;
  /// Entry type
  Type type;
  /// Sequence number
  std::uint16_t seqno;
  /// Cost to route to that entry
  std::uint16_t cost;
  /// Entry liveness
  boost::posix_time::ptime lastUpdate;
  /// Optional timer extensions
  mutable boost::shared_ptr<RoutingEntry::Timers> timers;
};

UNISPHERE_SHARED_POINTER(RoutingEntry)

/// RIB index tags
namespace RIBTags {
  class DestinationId;
  class TypeCost;
  class VportDestination;
}

// TODO: Migrate routing entries to shared pointers

typedef boost::multi_index_container<
  RoutingEntry,
  midx::indexed_by<
    // Index by destination identifier and order by cost within
    midx::ordered_non_unique<
      midx::tag<RIBTags::DestinationId>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, std::uint16_t, cost)
      >
    >,
    
    // Index by type and cost
    midx::ordered_non_unique<
      midx::tag<RIBTags::TypeCost>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, RoutingEntry::Type, type),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, std::uint16_t, cost)
      >
    >,

    // Index by origin vport
    midx::ordered_unique<
      midx::tag<RIBTags::VportDestination>,
      midx::composite_key<
        RoutingEntry,
        midx::const_mem_fun<RoutingEntry, Vport, &RoutingEntry::originVport>,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination)
      >
    >
  >
> RoutingInformationBase;

/// Bidirectional nodeId-vport mapping
typedef boost::bimap<NodeIdentifier, Vport> VportMap;

/// Mapping of identifiers to route originator descriptors
typedef std::unordered_map<NodeIdentifier, RouteOriginatorPtr> RouteOriginatorMap;

/**
 * Represents a landmark-relative address of the current node. Such an address
 * can be used by other nodes to route messages towards this node.
 */
class UNISPHERE_EXPORT LandmarkAddress {
public:
  /**
   * Constructs a landmark address with an empty routing path. Such an address
   * designates the landmark itself.
   *
   * @param landmarkId Landmark identifier
   */
  LandmarkAddress(const NodeIdentifier &landmarkId);

  /**
   * Constructs a landmark address.
   *
   * @param landmarkId Landmark identifier
   * @param path Reverse routing path (from landmark to node)
   */
  LandmarkAddress(const NodeIdentifier &landmarkId, const RoutingPath &path);

  /**
   * Returns the landmark identifier that can be used to route towards this
   * node.
   */
  const NodeIdentifier &landmarkId() const { return m_landmarkId; }

  /**
   * Returns the reverse routing path that can be used to route from the landmark
   * towards this node.
   */
  const RoutingPath &path() const { return m_path; }
private:
  /// Landmark identifier
  NodeIdentifier m_landmarkId;
  /// Reverse routing path
  RoutingPath m_path;
};

/**
 * The routing table data structure.
 */
class UNISPHERE_EXPORT CompactRoutingTable {
public:
  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   * @param localId Local node identifier
   * @param sizeEstimator A network size estimator
   */
  CompactRoutingTable(Context &context, const NodeIdentifier &localId,
    NetworkSizeEstimator &sizeEstimator);
  
  /**
   * Returns the currently active route to the given destination
   * based only on local information. If there is no known direct
   * route an invalid entry is returned.
   *
   * @param destination Destination address
   * @return A routing entry that can be used to forward to destination
   */
  const RoutingEntry &getActiveRoute(const NodeIdentifier &destination);

  /**
   * Returns a vport identifier corresponding to the given neighbor
   * identifier. If a vport has not yet been assigned, a new one is
   * assigned on the fly.
   *
   * @param neighbor Neighbor node identifier
   * @return Vport identifier
   */
  Vport getVportForNeighbor(const NodeIdentifier &neighbor);

  /**
   * Returns the neighbor identifier corresponding to the given
   * vport identifier. If this vport has not been assigned, a
   * null node identifier is returned.
   *
   * @param vport Vport identifier
   * @return Neighbor node identifier
   */
  NodeIdentifier getNeighborForVport(Vport vport) const;

  /**
   * Attempts to import a routing entry into the routing table.
   * 
   * @param entry Routing entry to import
   * @return True if routing table has been changed, false otherwise
   */
  bool import(const RoutingEntry &entry);

  /**
   * Retracts all routing entries for the specified destination.
   *
   * @param destination Destination identifier
   * @return True if routing table has changed, false otherwise
   */
  bool retract(const NodeIdentifier &destination);

  /**
   * Retract all routes originating from the specified vport (optionally
   * only for a specific destination).
   *
   * @param vport Virtual port identifier
   * @param destination Optional destination identifier
   * @return True if routing table has been changed, false otherwise
   */
  bool retract(Vport vport, const NodeIdentifier &destination = NodeIdentifier::INVALID);

  /**
   * Exports the full routing table to the specified peer.
   *
   * @param peer Peer to export the routing table to
   */
  void fullUpdate(const NodeIdentifier &peer);

  /**
   * Clears the whole routing table (RIB and vport mappings).
   */
  void clear();

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

  /**
   * Returns a list of landmark-relative local addresses.
   *
   * @param count Number of addresses to return
   * @return A list of landmark addresses
   */
  std::list<LandmarkAddress> getLocalAddresses(size_t count = 1);

  /**
   * Outputs the routing table to a stream.
   *
   * @param stream Output stream to dump into
   */
  void dump(std::ostream &stream);
public:
  /// Signal that gets called when a routing entry should be exported to neighbours
  boost::signal<void(const RoutingEntry&, const NodeIdentifier&)> signalExportEntry;
  /// Signal that gets called when a routing entry should be retracted to neighbours
  boost::signal<void(const RoutingEntry&)> signalRetractEntry;
  /// Signal that gets called when the local address changes
  boost::signal<void(const LandmarkAddress&)> signalAddressChanged;
protected:
  /**
   * Returns the maximum vicinity size.
   */
  size_t getMaximumVicinitySize() const;

  /**
   * Returns the size of the current vicinity together with the routing entry
   * of a destination with the largest minimum cost.
   *
   * @return A tuple (size, entry)
   */
  boost::tuple<size_t, RoutingEntry> getCurrentVicinity() const;

  /**
   * Returns the number of landmarks in the routing table.
   */
  size_t getLandmarkCount() const;

  /**
   * Called when the freshness timer expires on a direct routing entry.
   *
   * @param error Boost error code (in case timer was interrupted)
   * @param entry Routing entry that expired
   */
  void entryTimerExpired(const boost::system::error_code &error,
    const RoutingEntry &entry);
private:
  /// UNISPHERE context
  Context &m_context;
  /// Local node identifier
  NodeIdentifier m_localId;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Mutex protecting the routing table
  std::recursive_mutex m_mutex;
  /// Information needed for routing to any node
  RoutingInformationBase m_rib;
  /// Route originator mapping
  RouteOriginatorMap m_originatorMap;
  /// Vport mapping for direct routes
  VportMap m_vportMap;
  /// Vport index counter
  Vport m_nextVport;
  /// Local address based on nearest landmark; multiple for redundancy?
  std::list<LandmarkAddress> m_localAddress;
  /// Landmark status of the current node
  bool m_landmark;
};
  
}

#endif
