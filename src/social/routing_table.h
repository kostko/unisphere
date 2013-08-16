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

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>

#include "core/context.h"
#include "core/signal.h"
#include "identity/node_identifier.h"
#include "social/size_estimator.h"
#include "social/address.h"

namespace UniSphere {

/**
 * A descriptor for a remote node that originates routes. It is used
 * to keep track of its sequence numbers when routing entries get
 * removed.
 */
class UNISPHERE_EXPORT RouteOriginator {
public:
  /**
   * Class constructor.
   *
   * @param nodeId Originator node identifier
   */
  RouteOriginator(const NodeIdentifier &nodeId);

  /**
   * Returns true if given sequence number is newer than the currently
   * stored for this route originator.
   *
   * @param seq Sequence number to check against
   * @return True if given sequence number is newer, false otherwise
   */
  bool isNewer(std::uint16_t seq) const;

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

/**
 * An entry in the compact routing table.
 */
class UNISPHERE_EXPORT RoutingEntry {
public:
  /**
   * Constructs a routing entry.
   *
   * @param context UNISPHERE context
   * @param destination Destination node identifier
   * @param landmark Landmark status of the entry
   * @param seqno Sequence number
   */
  RoutingEntry(Context &context,
               const NodeIdentifier &destination,
               bool landmark,
               std::uint16_t seqno);

  /**
   * Class destructor.
   */
  ~RoutingEntry();

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
  Vport originVport() const { return forwardPath.front(); }

  /**
   * Returns the length of the forward path.
   */
  size_t hops() const { return forwardPath.size(); }

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
  RouteOriginatorPtr originator;
  /// Destination node identifier
  NodeIdentifier destination;
  /// Path of vports to destination
  RoutingPath forwardPath;
  /// Path of vports from destination (only for landmarks)
  RoutingPath reversePath;
  /// Entry type
  bool landmark;
  /// Vicinity status
  bool vicinity;
  /// Extended vicinity status
  bool extendedVicinity;
  /// Sequence number
  std::uint16_t seqno;
  /// Cost to route to that entry
  std::uint16_t cost;
  /// Active mark
  bool active;
  /// Entry liveness
  boost::posix_time::ptime lastUpdate;
  /// Expiration timer
  boost::asio::deadline_timer expiryTimer;
};

UNISPHERE_SHARED_POINTER(RoutingEntry)

class SloppyGroupManager;

/**
 * The routing table data structure.
 */
class UNISPHERE_EXPORT CompactRoutingTable {
public:
  /// Local address redundancy
  static const int local_address_redundancy = 3;
  /// Tags for topology dump graph properties.
  struct TopologyDumpTags {
    /// Specifies the node's name
    struct NodeName { typedef boost::vertex_property_tag kind; };
    /// Specifies the node's landmark status
    struct NodeIsLandmark { typedef boost::vertex_property_tag kind; };
    /// Node state size
    struct NodeStateSize { typedef boost::vertex_property_tag kind; };
    /// Link's vport identifier
    struct LinkVportId { typedef boost::edge_property_tag kind; };
    /// Link weight
    struct LinkWeight { typedef boost::edge_property_tag kind; };
  };

  /// Graph definition for dumping the compact routing topology into
  typedef boost::labeled_graph<
    boost::adjacency_list<
      boost::vecS,
      boost::vecS,
      boost::bidirectionalS,
      boost::property<TopologyDumpTags::NodeName, std::string,
        boost::property<TopologyDumpTags::NodeIsLandmark, int,
          boost::property<TopologyDumpTags::NodeStateSize, int>>>,
      boost::property<TopologyDumpTags::LinkVportId, Vport,
        boost::property<TopologyDumpTags::LinkWeight, int>>
    >,
    std::string,
    boost::hash_mapS
  > TopologyDumpGraph;

  /// A helper structure for returning longest prefix matches
  struct LongestPrefixMatch {
    /// Node identifier of the longest prefix match node in vicinity
    NodeIdentifier nodeId;
    /// Next hop in route towards the longest prefix match node
    NodeIdentifier nextHop;
  };

  /// A helper structure for returning vicinity descriptors
  struct VicinityDescriptor {
    /// Destination node identifier
    NodeIdentifier nodeId;
    /// Number of hops to reach the destination
    size_t hops;
  };

  /**
   * A structure for reporting routing table statistics.
   */
  struct Statistics {
    /// Number of route updates
    size_t routeUpdates = 0;
    /// Number of route expirations
    size_t routeExpirations = 0;
  };

  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   * @param localId Local node identifier
   * @param sizeEstimator A network size estimator
   * @param sloppyGroup Sloppy group manager
   */
  CompactRoutingTable(Context &context,
                      const NodeIdentifier &localId,
                      NetworkSizeEstimator &sizeEstimator,
                      SloppyGroupManager &sloppyGroup);

  CompactRoutingTable(const CompactRoutingTable&) = delete;
  CompactRoutingTable &operator=(const CompactRoutingTable&) = delete;
  
  /**
   * Returns the currently active route to the given destination
   * based only on local information. If there is no known direct
   * route an invalid entry is returned.
   *
   * @param destination Destination address
   * @return Node identifier of the next hop
   */
  NodeIdentifier getActiveRoute(const NodeIdentifier &destination);

  /**
   * Returns the routing entry with the longest prefix match in
   * its node identifier.
   *
   * @param destination Destiantion address
   * @return Descriptor for the longest prefix match
   */
  LongestPrefixMatch getLongestPrefixMatch(const NodeIdentifier &destination);

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
  bool import(RoutingEntryPtr entry);

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
  bool retract(Vport vport,
               const NodeIdentifier &destination = NodeIdentifier::INVALID);

  /**
   * Exports the full routing table to the specified peer.
   *
   * @param peer Peer to export the routing table to
   */
  void fullUpdate(const NodeIdentifier &peer);

  /**
   * Returns the number of all records that are being stored in the routing
   * table.
   */
  size_t size() const;

  /**
   * Returns the number of active routing entries.
   */
  size_t sizeActive() const;

  /**
   * Returns the number of routing entries from node's vicinity.
   */
  size_t sizeVicinity() const;

  /**
   * Returns a list of vicinity descriptors for the node's vicinity.
   */
  std::list<VicinityDescriptor> getVicinity() const;

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
  bool isLandmark() const;

  /**
   * Returns a list of landmark-relative local addresses.
   *
   * @return A list of landmark addresses
   */
  LandmarkAddressList getLocalAddresses() const;

  /**
   * Returns the first local address.
   */
  LandmarkAddress getLocalAddress() const;

  /**
   * Returns various statistics about routing table operation.
   */
  const Statistics &statistics() const;

  /**
   * Outputs the routing table to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream,
            std::function<std::string(const NodeIdentifier&)> resolve = nullptr) const;

  /**
   * Dumps the locally known routing topology into a graph.
   *
   * @param graph Output graph to dump into
   * @param resolve Optional name resolver
   */
  void dumpTopology(TopologyDumpGraph &graph,
                    std::function<std::string(const NodeIdentifier&)> resolve = nullptr);
public:
  /// Signal that gets called when a routing entry should be exported to neighbours. This
  /// signal is called with the internal mutex held, so handlers MUST avoid any operations
  /// that acquire additional mutexes, otherwise deadlocks might ocurr!
  DeferrableSignal<void(RoutingEntryPtr, const NodeIdentifier&)> signalExportEntry;
  /// Signal that gets called when a routing entry should be retracted to neighbours. This
  /// signal is called with the internal mutex held, so handlers MUST avoid any operations
  /// that acquire additional mutexes, otherwise deadlocks might ocurr!
  DeferrableSignal<void(RoutingEntryPtr)> signalRetractEntry;
  /// Signal that gets called when the local address changes (deferred)
  DeferrableSignal<void(const LandmarkAddressList&)> signalAddressChanged;
  /// Signal that gets called when a new landmark is learned (deferred)
  DeferrableSignal<void(const NodeIdentifier&)> signalLandmarkLearned;
  /// Signal that gets called when a landmark is removed (deferred)
  DeferrableSignal<void(const NodeIdentifier&)> signalLandmarkRemoved;
  /// Signal that gets called when a new vicinity node is learned (deferred)
  DeferrableSignal<void(const VicinityDescriptor&)> signalVicinityLearned;
  /// Signal that gets called when a new vicinity node is removed (deferred)
  DeferrableSignal<void(const NodeIdentifier&)> signalVicinityRemoved;
private:
  UNISPHERE_DECLARE_PRIVATE(CompactRoutingTable)
};
  
}

#endif
