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
#include "social/routing_table.h"
#include "social/compact_router.h"
#include "social/sloppy_group.h"

#include <unordered_map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/tuple/tuple.hpp>

namespace midx = boost::multi_index;

namespace UniSphere {

/// RIB index tags
namespace RIBTags {
  class DestinationId;
  class ActiveRoutes;
  class LandmarkCost;
  class LandmarkDestination;
  class Vicinity;
  class VicinityAny;
  class VportDestination;
}

typedef boost::multi_index_container<
  RoutingEntryPtr,
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

    // Indey by activeness and destination identifier
    midx::ordered_non_unique<
      midx::tag<RIBTags::ActiveRoutes>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, active),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination)
      >
    >,

    // Index by landmark status and cost
    midx::ordered_non_unique<
      midx::tag<RIBTags::LandmarkCost>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, landmark),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, std::uint16_t, cost)
      >
    >,
    
    // Index by landmark status and destination
    midx::ordered_non_unique<
      midx::tag<RIBTags::LandmarkDestination>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, landmark),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination)
      >
    >,

    // Index by vicinity and extended vicinity
    midx::ordered_non_unique<
      midx::tag<RIBTags::Vicinity>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, vicinity),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, extendedVicinity),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        midx::const_mem_fun<RoutingEntry, size_t, &RoutingEntry::hops>
      >
    >,

    // Index by vicinity
    midx::ordered_non_unique<
      midx::tag<RIBTags::VicinityAny>,
      midx::composite_key<
        RoutingEntry,
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, bool, vicinity),
        BOOST_MULTI_INDEX_MEMBER(RoutingEntry, NodeIdentifier, destination),
        midx::const_mem_fun<RoutingEntry, size_t, &RoutingEntry::hops>
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
typedef boost::bimap<
  boost::bimaps::unordered_set_of<NodeIdentifier>,
  boost::bimaps::unordered_set_of<Vport>
> VportMap;

/// Mapping of identifiers to route originator descriptors
typedef std::unordered_map<NodeIdentifier, RouteOriginatorPtr> RouteOriginatorMap;

class CompactRoutingTablePrivate {
public:
  /**
   * Current vicinity descriptor.
   */
  struct CurrentVicinity {
    /// Current vicinity size
    size_t size;
    /// Routing entry with the largest hop count within the vicinity
    /// (retract candidate in case of overflows)
    RoutingEntryPtr maxHopEntry;
    /// Iterator pointing to the maxHopEntry
    RoutingInformationBase::index<RIBTags::Vicinity>::type::iterator maxHopIterator;
  };

  /**
   * Sloppy group bucket descriptor.
   */
  struct SloppyGroupBucket {
    /// Current bucket size
    size_t size;
    /// Routing entry with the largest hop count within the bucket
    /// (retract candidate in case of overflows)
    RoutingEntryPtr maxHopEntry;
    /// Iterator pointing to the maxHopEntry
    RoutingInformationBase::index<RIBTags::VicinityAny>::type::iterator maxHopIterator;
  };

  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   * @param localId Local node identifier
   * @param sizeEstimator A network size estimator
   * @param sloppyGroup Sloppy group manager
   * @param rt Public instance
   */
  CompactRoutingTablePrivate(Context &context,
                             const NodeIdentifier &localId,
                             NetworkSizeEstimator &sizeEstimator,
                             SloppyGroupManager &sloppyGroup,
                             CompactRoutingTable &rt);
  
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
   * Returns the routing entry that can be used as a relay when
   * the destination identifier can't be resolved locally.
   *
   * @param destination Destiantion address
   * @return Descriptor for the sloppy group relay
   */
  CompactRoutingTable::SloppyGroupRelay getSloppyGroupRelay(const NodeIdentifier &destination);

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
  void dumpTopology(CompactRoutingTable::TopologyDumpGraph &graph,
                    std::function<std::string(const NodeIdentifier&)> resolve = nullptr);

  /**
   * Returns the maximum vicinity size.
   */
  size_t getMaximumVicinitySize() const;

  /**
   * Returns the maximum extended vicinity bucket size.
   */
  size_t getMaximumBucketSize() const;

  /**
   * Returns the size of the current vicinity together with the routing entry
   * with the largest hop count within the vicinity.
   *
   * @return Current vicinity descriptor
   */
  CurrentVicinity getCurrentVicinity() const;

  /**
   * Returns the size of an extended vicinity bucket together with the routing
   * entry with the largest hop count within the bucket.
   *
   * @return Sloppy group bucket descriptor
   */
  SloppyGroupBucket getSloppyGroupBucket(const NodeIdentifier &nodeId) const;

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
                         RoutingEntryWeakPtr entry);

  /**
   * Notifies the router that an entry should be exported to neighbor
   * nodes.
   *
   * @param entry Routing entry to export
   * @param peer Optional peer to limit the export to
   */
  void exportEntry(RoutingEntryPtr entry,
                   const NodeIdentifier &peer = NodeIdentifier::INVALID);

  /**
   * Performs best route selection for a specific destination and
   * activates the best route (if found).
   *
   * @param destination Destination identifier
   * @return Tuple (activated, new_best, old_best), where activated is true if a
   *   route was activated, false otherwise; in case activated is true, new_best
   *   contains the routing entry that was activated while old_best contains the
   *   previously active entry or null if there was no such entry before
   */
  boost::tuple<bool, RoutingEntryPtr, RoutingEntryPtr> selectBestRoute(const NodeIdentifier &destination);

  /**
   * Performs local address selection.
   *
   * @return True when local address has changed, false otherwise
   */
  bool selectLocalAddress();
public:
  UNISPHERE_DECLARE_PUBLIC(CompactRoutingTable)

  /// UNISPHERE context
  Context &m_context;
  /// Logger instance
  Logger m_logger;
  /// Local node identifier
  NodeIdentifier m_localId;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
  /// Sloppy group manager
  SloppyGroupManager &m_sloppyGroup;
  /// Mutex protecting the routing table
  mutable std::recursive_mutex m_mutex;
  /// Information needed for routing to any node
  RoutingInformationBase m_rib;
  /// Route originator mapping
  RouteOriginatorMap m_originatorMap;
  /// Vport mapping for direct routes
  VportMap m_vportMap;
  /// Vport index counter
  Vport m_nextVport;
  /// Local address based on nearest landmark (multiple for redundancy)
  LandmarkAddressList m_localAddress;
  /// Landmark status of the current node
  bool m_landmark;

  /// Statistics
  CompactRoutingTable::Statistics m_statistics;
};

RouteOriginator::RouteOriginator(const NodeIdentifier &nodeId)
  : identifier(nodeId),
    seqno(0),
    smallestCost(0xFFFF)
{
}

bool RouteOriginator::isNewer(std::uint16_t seq) const
{
  return (seqno - seq) & 0x8000;
}

boost::posix_time::time_duration RouteOriginator::age() const
{
  return boost::posix_time::microsec_clock::universal_time() - lastUpdate;
}

RoutingEntry::RoutingEntry(Context &context,
                           const NodeIdentifier &destination,
                           bool landmark,
                           std::uint16_t seqno)
  : destination(destination),
    landmark(landmark),
    seqno(seqno),
    active(false),
    expiryTimer(context.service())
{
}

RoutingEntry::~RoutingEntry()
{
}

bool RoutingEntry::isFeasible() const
{
  // If originator is not yet defined, this entry is feasible
  if (!originator)
    return true;

  // If sequence number is greater, this entry is feasible
  if (originator->isNewer(seqno))
    return true;

  // If cost is strictly smaller than known minimum cost, this entry is feasible
  if (cost < originator->smallestCost)
    return true;

  return false;
}

boost::posix_time::time_duration RoutingEntry::age() const
{
  return boost::posix_time::microsec_clock::universal_time() - lastUpdate;
}

bool RoutingEntry::operator==(const RoutingEntry &other) const
{
  return destination == other.destination && landmark == other.landmark && seqno == other.seqno &&
    cost == other.cost && forwardPath == other.forwardPath && reversePath == other.reversePath;
}

CompactRoutingTablePrivate::CompactRoutingTablePrivate(Context &context,
                                                       const NodeIdentifier &localId,
                                                       NetworkSizeEstimator &sizeEstimator,
                                                       SloppyGroupManager &sloppyGroup,
                                                       CompactRoutingTable &rt)
  : q(rt),
    m_context(context),
    m_logger(logging::keywords::channel = "routing_table"),
    m_localId(localId),
    m_sizeEstimator(sizeEstimator),
    m_sloppyGroup(sloppyGroup),
    m_nextVport(0),
    m_landmark(false)
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(localId));
}

Vport CompactRoutingTablePrivate::getVportForNeighbor(const NodeIdentifier &neighbor)
{
  RecursiveUniqueLock lock(m_mutex);

  VportMap::left_const_iterator i = m_vportMap.left.find(neighbor);
  if (i == m_vportMap.left.end()) {
    // No vport has been assigned yet, create a new mapping
    m_vportMap.insert(VportMap::value_type(neighbor, m_nextVport));
    return m_nextVport++;
  }

  return (*i).second;
}

NodeIdentifier CompactRoutingTablePrivate::getNeighborForVport(Vport vport) const
{
  VportMap::right_const_iterator i = m_vportMap.right.find(vport);
  if (i == m_vportMap.right.end())
    return NodeIdentifier();

  return (*i).second;
}

size_t CompactRoutingTablePrivate::getMaximumVicinitySize() const
{
  // TODO: This is probably not the best way (int -> double -> sqrt -> int)
  double n = static_cast<double>(m_sizeEstimator.getNetworkSize());
  return static_cast<size_t>(std::sqrt(n * std::log(n)));
}

size_t CompactRoutingTablePrivate::getMaximumBucketSize() const
{
  // TODO: This is probably not the best way (int -> double -> log -> int)
  double n = static_cast<double>(m_sizeEstimator.getNetworkSize());
  return static_cast<size_t>(std::log(n));
}

CompactRoutingTablePrivate::CurrentVicinity CompactRoutingTablePrivate::getCurrentVicinity() const
{
  RecursiveUniqueLock lock(m_mutex);
  auto entries = m_rib.get<RIBTags::Vicinity>().equal_range(boost::make_tuple(true, false));
  NodeIdentifier lastDestination;
  CurrentVicinity vicinity;
  vicinity.size = 0;

  // Count the number of unique destinations in the vicinity
  for (auto it = entries.first; it != entries.second; ++it) {
    if ((*it)->destination != lastDestination) {
      vicinity.size++;
      if (!vicinity.maxHopEntry || (*it)->hops() >= vicinity.maxHopEntry->hops()) {
        vicinity.maxHopEntry = *it;
        vicinity.maxHopIterator = it;
      }
    }

    lastDestination = (*it)->destination;
  }

  return vicinity;
}

CompactRoutingTablePrivate::SloppyGroupBucket CompactRoutingTablePrivate::getSloppyGroupBucket(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);
  NodeIdentifier groupStart = nodeId.prefix(m_sloppyGroup.getGroupPrefixLength());
  NodeIdentifier groupEnd = nodeId.prefix(m_sloppyGroup.getGroupPrefixLength(), 0xFF);

  auto &ribVicinity = m_rib.get<RIBTags::VicinityAny>();
  auto begin = ribVicinity.lower_bound(boost::make_tuple(true, groupStart));
  auto end = ribVicinity.upper_bound(boost::make_tuple(true, groupEnd));
  NodeIdentifier lastDestination;
  SloppyGroupBucket bucket;
  bucket.size = 0;

  for (auto it = begin; it != end; ++it) {
    if ((*it)->destination != lastDestination) {
      bucket.size++;
      // Only consider entries in extended vicinity for removal
      if ((*it)->extendedVicinity && 
          (!bucket.maxHopEntry || (*it)->hops() >= bucket.maxHopEntry->hops())) {
        bucket.maxHopEntry = *it;
        bucket.maxHopIterator = it;
      }
    }

    lastDestination = (*it)->destination;
  }

  return bucket;
}

size_t CompactRoutingTablePrivate::getLandmarkCount() const
{
  RecursiveUniqueLock lock(m_mutex);
  auto entries = m_rib.get<RIBTags::LandmarkDestination>().equal_range(true);
  NodeIdentifier lastDestination;
  size_t landmarkCount = 0;

  // Count the number of unique destinations in the landmark set
  for (auto it = entries.first; it != entries.second; ++it) {
    if ((*it)->destination != lastDestination)
      landmarkCount++;

    lastDestination = (*it)->destination;
  }

  return landmarkCount;
}

void CompactRoutingTablePrivate::entryTimerExpired(const boost::system::error_code &error,
                                                   RoutingEntryWeakPtr entry)
{
  if (error)
    return;

  if (RoutingEntryPtr e = entry.lock()) {
    // Retract the entry from the routing table
    m_statistics.routeExpirations++;
    retract(e->originVport(), e->destination);
  }
}

void CompactRoutingTablePrivate::fullUpdate(const NodeIdentifier &peer)
{
  RecursiveUniqueLock lock(m_mutex);

  // Export all active routes to the selected peer
  auto entries = m_rib.get<RIBTags::ActiveRoutes>().equal_range(true);
  for (auto it = entries.first; it != entries.second; ++it) {
    exportEntry(*it, peer);
  }
}

bool CompactRoutingTablePrivate::import(RoutingEntryPtr entry)
{
  RecursiveUniqueLock lock(m_mutex);

  assert(entry);
  if (entry->destination == m_localId)
    return false;

  if (!entry->originator) {
    // Discover the originator for this entry
    auto it = m_originatorMap.find(entry->destination);
    if (it == m_originatorMap.end()) {
      RouteOriginatorPtr nro(new RouteOriginator(entry->destination));
      entry->originator = nro;
      m_originatorMap.insert({{entry->destination, nro}});
    } else {
      entry->originator = it->second;
    }
  }

  // Compute cost based on hop count and set entry timestamp
  entry->cost = entry->forwardPath.size();
  entry->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  // Call hooks that can filter the entry
  if (!q.signalImportEntry(entry))
    return false;

  // Check if an entry to the same destination from the same vport already exists; in
  // this case, the announcement counts as an implicit retract
  bool landmarkChangedType = false;
  auto &ribVport = m_rib.get<RIBTags::VportDestination>();
  auto existing = ribVport.find(boost::make_tuple(entry->originVport(), entry->destination));
  if (existing != ribVport.end()) {
    // Ignore import when the existing entry is the same as the new one
    if (**existing == *entry) {
      // Update the entry's last seen timestamp
      BOOST_ASSERT(ribVport.modify(existing, [&](RoutingEntryPtr e) {
        e->lastUpdate = entry->lastUpdate;

        // Restart expiry timer
        if (e->expiryTimer.expires_from_now(m_context.roughly(CompactRouter::interval_neighbor_expiry)) > 0) {
          e->expiryTimer.async_wait(boost::bind(&CompactRoutingTablePrivate::entryTimerExpired, this, _1, RoutingEntryWeakPtr(e)));
        }
      }));
      return false;
    }

    // Update certain attributes of the routing entry
    BOOST_ASSERT(ribVport.modify(existing, [&](RoutingEntryPtr e) {
      if (e->landmark != entry->landmark)
        landmarkChangedType = true;

      // TODO: In some cases modification can cause the entry to be retracted; for example
      // when it is no longer a landmark and it doesn't fall into the (extended) vicinity

      e->forwardPath = entry->forwardPath;
      e->reversePath = entry->reversePath;
      e->landmark = entry->landmark;
      e->seqno = entry->seqno;
      e->cost = entry->cost;
      e->lastUpdate = entry->lastUpdate;

      // Restart expiry timer
      if (e->expiryTimer.expires_from_now(m_context.roughly(CompactRouter::interval_neighbor_expiry)) > 0) {
        e->expiryTimer.async_wait(boost::bind(&CompactRoutingTablePrivate::entryTimerExpired, this, _1, RoutingEntryWeakPtr(e)));
      }
    }));
  } else {
    // An entry should be inserted if it represents a landmark or if it falls into the vicinity
    // or the extended vicinity (based on sloppy group membership) of the current node
    bool isVicinity = false;
    bool isExtendedVicinity = false;
    // Fetch current vicinity
    CurrentVicinity vicinity = getCurrentVicinity();

    if (vicinity.size >= getMaximumVicinitySize()) {
      if (vicinity.maxHopEntry->hops() > entry->hops()) {
        // In vicinity, but we might need to retract an entry if it isn't in any bucket
        isVicinity = true;
        // Modify maxHopEntry as it is no longer in vicinity (but is in extended vicinity)
        m_rib.get<RIBTags::Vicinity>().modify(vicinity.maxHopIterator, [&](RoutingEntryPtr e) {
          e->extendedVicinity = true;
        });
        // Fetch corresponding SG bucket
        SloppyGroupBucket bucket = getSloppyGroupBucket(entry->destination);
        // Retract entry when bucket would overflow
        if (bucket.size >= getMaximumBucketSize() && bucket.maxHopEntry) {
          if (bucket.maxHopEntry->landmark) {
            // Landmark entries should never be removed, so they are moved from vicinity
            m_rib.get<RIBTags::VicinityAny>().modify(bucket.maxHopIterator, [&](RoutingEntryPtr e) {
              e->vicinity = false;
              e->extendedVicinity = false;
            });
          } else {
            retract(bucket.maxHopEntry->destination);
          }
        }
      } else {
        // Not part of standard vicinity, but might still be accepted if it belongs
        // to the sloppy group we need
        SloppyGroupBucket bucket = getSloppyGroupBucket(entry->destination);
        if (bucket.size >= getMaximumBucketSize()) {
          // Check if there is any entry that can be removed
          if (bucket.maxHopEntry && bucket.maxHopEntry->hops() > entry->hops()) {
            isVicinity = true;
            isExtendedVicinity = true;
            
            // Retract the entry to make room for new one
            if (bucket.maxHopEntry->landmark) {
              // Landmark entries should never be removed, so they are moved from vicinity
              m_rib.get<RIBTags::VicinityAny>().modify(bucket.maxHopIterator, [&](RoutingEntryPtr e) {
                e->vicinity = false;
                e->extendedVicinity = false;
              });
            } else {
              retract(bucket.maxHopEntry->destination);
            }
          }
        } else {
          isVicinity = true;
          isExtendedVicinity = true;
        }
      }
    } else {
      // Inside vicinity as it is not yet full
      isVicinity = true;
    }

    if (!isVicinity && !entry->landmark)
      return false;
    entry->vicinity = isVicinity;
    entry->extendedVicinity = isExtendedVicinity;

    m_rib.insert(entry);

    // Setup expiry timer for the routing entry
    entry->expiryTimer.expires_from_now(m_context.roughly(CompactRouter::interval_neighbor_expiry));
    entry->expiryTimer.async_wait(boost::bind(&CompactRoutingTablePrivate::entryTimerExpired, this, _1, RoutingEntryWeakPtr(entry)));
  }

  // Determine what the new best route for this destination is
  RoutingEntryPtr newBest, oldBest;
  boost::tie(boost::tuples::ignore, newBest, oldBest) = selectBestRoute(entry->destination);

  if (newBest == oldBest && entry == newBest && landmarkChangedType) {
    // Landmark type of the currently active route has changed
    if (entry->landmark)
      q.signalLandmarkLearned.defer(entry->destination);
    else
      q.signalLandmarkRemoved.defer(entry->destination);
  }

  // Determine whether local address has changed
  if (entry->landmark || landmarkChangedType)
    selectLocalAddress();

  return true;
}

void CompactRoutingTablePrivate::exportEntry(RoutingEntryPtr entry, const NodeIdentifier &peer)
{
  RouteOriginatorPtr orig = entry->originator;

  // Update route originator sequence number and liveness
  if (orig->age().total_seconds() > CompactRouter::origin_expiry_time ||
      orig->isNewer(entry->seqno) ||
      (entry->seqno == orig->seqno && entry->cost < orig->smallestCost)) {
    orig->seqno = entry->seqno;
    orig->smallestCost = entry->cost + 1;
  }
  orig->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  // Export the entry
  q.signalExportEntry(entry, peer);
}

boost::tuple<bool, RoutingEntryPtr, RoutingEntryPtr> CompactRoutingTablePrivate::selectBestRoute(const NodeIdentifier &destination)
{
  auto &ribDestination = m_rib.get<RIBTags::DestinationId>();
  auto entries = ribDestination.equal_range(boost::make_tuple(destination));
  auto oldBest = ribDestination.end();
  auto newBest = ribDestination.end();

  // If there are no routes to choose from, return early
  if (entries.first == entries.second)
    return boost::make_tuple(false, RoutingEntryPtr(), RoutingEntryPtr());

  // Finds the first feasible route with minimum cost
  for (auto it = entries.first; it != entries.second; ++it) {
    RoutingEntryPtr e = *it;
    if (e->active)
      oldBest = it;

    // Scan forward to find the old best route in order to deactivate it
    if (newBest != ribDestination.end()) {
      if (oldBest == ribDestination.end())
        continue;
      else
        break;
    }

    if (!e->isFeasible())
      continue;

    newBest = it;
  }

  // If there are no feasible routes, return early
  if (newBest == ribDestination.end()) {
    // TODO: Request a new sequence number from route originator
    BOOST_LOG_SEV(m_logger, log::warning) << "No feasible routes towards " << destination.hex() << ".";
    return boost::make_tuple(false, RoutingEntryPtr(), RoutingEntryPtr());
  }

  RoutingEntryPtr newBestEntry = *newBest;
  RoutingEntryPtr oldBestEntry;
  if (oldBest != ribDestination.end())
    oldBestEntry = *oldBest;

  if (newBest != oldBest) {
    // Update the routing table
    if (oldBest != ribDestination.end()) {
      ribDestination.modify(oldBest, [&](RoutingEntryPtr e) {
        e->active = false;
      });
    }

    ribDestination.modify(newBest, [&](RoutingEntryPtr e) {
      e->active = true;
    });

    // Check if any landmarks have changed status because of this update
    if (oldBestEntry) {
      // Check if landmark type for the active entry has changed
      if (oldBestEntry->landmark && !newBestEntry->landmark)
        q.signalLandmarkRemoved.defer(destination);
      else if (!oldBestEntry->landmark && newBestEntry->landmark)
        q.signalLandmarkLearned.defer(destination);

      // Check if vicinity type for the active entry has changed
      if (oldBestEntry->vicinity && !newBestEntry->vicinity)
        q.signalVicinityRemoved.defer(destination);
      else if (!oldBestEntry->vicinity && newBestEntry->vicinity)
        q.signalVicinityLearned.defer(CompactRoutingTable::VicinityDescriptor{ newBestEntry->destination, newBestEntry->hops() });
    } else {
      // Check if a landmark has become the active route
      if (newBestEntry->landmark)
        q.signalLandmarkLearned.defer(destination);

      // Check if a vicinity member has become the active route
      if (newBestEntry->vicinity)
        q.signalVicinityLearned.defer(CompactRoutingTable::VicinityDescriptor{ newBestEntry->destination, newBestEntry->hops() });
    }

    m_statistics.routeUpdates++;

    // Export new active route to neighbors
    exportEntry(newBestEntry);
  }

  return boost::make_tuple(true, *newBest, oldBestEntry);
}

bool CompactRoutingTablePrivate::selectLocalAddress()
{
  if (m_landmark) {
    if (m_localAddress.empty() ||
        m_localAddress.front().landmarkId() != m_localId ||
        m_localAddress.size() > 1) {
      m_localAddress.clear();
      m_localAddress.push_back(LandmarkAddress(m_localId));
      q.signalAddressChanged.defer(m_localAddress);
      return true;
    }
    
    return false;
  }

  auto ait = m_localAddress.begin();
  auto entries = m_rib.get<RIBTags::LandmarkCost>().equal_range(true);
  bool changed = false;
  for (auto it = entries.first; it != entries.second; ++it) {
    LandmarkAddress lrAddress((*it)->destination, (*it)->reversePath);

    if (ait == m_localAddress.end()) {
      if (m_localAddress.size() >= CompactRoutingTable::local_address_redundancy)
        break;

      // Address inserted at the end of list
      changed = true;
      m_localAddress.push_back(lrAddress);
      continue;
    }

    if (*ait != lrAddress) {
      // Address has changed position
      *ait = lrAddress;
      changed = true;
    }

    ++ait;
  }
  
  if (changed)
    q.signalAddressChanged.defer(m_localAddress);
  return changed;
}

NodeIdentifier CompactRoutingTablePrivate::getActiveRoute(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribActive =  m_rib.get<RIBTags::ActiveRoutes>();
  auto entry = ribActive.find(boost::make_tuple(true, destination));
  if (entry == ribActive.end())
    return NodeIdentifier::INVALID;

  return getNeighborForVport((*entry)->originVport());
}

CompactRoutingTable::SloppyGroupRelay CompactRoutingTablePrivate::getSloppyGroupRelay(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  // If the RIB is empty, return a null entry
  if (m_rib.empty())
    return CompactRoutingTable::SloppyGroupRelay();

  NodeIdentifier groupStart = destination.prefix(m_sloppyGroup.getGroupPrefixLength());
  NodeIdentifier groupEnd = destination.prefix(m_sloppyGroup.getGroupPrefixLength(), 0xFF);

  auto &ribVicinity = m_rib.get<RIBTags::VicinityAny>();
  auto begin = ribVicinity.lower_bound(boost::make_tuple(true, groupStart));
  auto end = ribVicinity.upper_bound(boost::make_tuple(true, groupEnd));
  RoutingEntryPtr bestEntry;
  size_t bestHops = -1;

  // TODO: We could also cache the best entry for a given sloppy group, so we don't need
  //       to query the RIB on each such decision?
  for (auto it = begin; it != end; ++it) {
    RoutingEntryPtr entry = *it;
    if (!entry->active)
      continue;

    if (entry->hops() < bestHops) {
      bestEntry = entry;
      bestHops = entry->hops();
    }
  }

  return CompactRoutingTable::SloppyGroupRelay{
    bestEntry->destination,
    getNeighborForVport(bestEntry->originVport())
  };
}

bool CompactRoutingTablePrivate::retract(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribDestination = m_rib.get<RIBTags::DestinationId>();
  auto entries = ribDestination.equal_range(boost::make_tuple(destination));
  if (entries.first == entries.second)
    return false;

  bool wasLandmark = false;

  for (auto it = entries.first; it != entries.second;) {
    RoutingEntryPtr entry = *it;
    if (entry->landmark)
      wasLandmark = true;

    it = ribDestination.erase(it);

    // Send retractions for active entries
    if (entry->active)
      q.signalRetractEntry(entry);
  }

  // We have just unlearned a vicinity member or landmark
  if (wasLandmark)
    q.signalLandmarkRemoved.defer(destination);
  else
    q.signalVicinityRemoved.defer(destination);

  return true;
}

bool CompactRoutingTablePrivate::retract(Vport vport, const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribVport = m_rib.get<RIBTags::VportDestination>();
  decltype(ribVport.equal_range(boost::make_tuple())) routes;

  if (destination.isNull()) {
    // Retract all routes going via the specified vport
    routes = ribVport.equal_range(boost::make_tuple(vport));
  } else {
    // Only retract the route going via the specified vport and to the specified
    // destination address
    routes = ribVport.equal_range(boost::make_tuple(vport, destination));
  }

  // Erase selected entries and then export/retract routes for removed destinations
  for (auto it = routes.first; it != routes.second;) {
    RoutingEntryPtr entry = *it;

    it = ribVport.erase(it);

    // If entry was part of an active route, we must determine a new active route for this destination
    if (entry->active) {
      if (!selectBestRoute(entry->destination).get<0>())
        q.signalRetractEntry(entry);
    }
  }

  return false;
}

size_t CompactRoutingTablePrivate::size() const
{
  return m_rib.size();
}

size_t CompactRoutingTablePrivate::sizeActive() const
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribActive =  m_rib.get<RIBTags::ActiveRoutes>();
  return ribActive.count(true);
}

size_t CompactRoutingTablePrivate::sizeVicinity() const
{
  RecursiveUniqueLock lock(m_mutex);
  return getCurrentVicinity().size;
}

void CompactRoutingTablePrivate::clear()
{
  RecursiveUniqueLock lock(m_mutex);

  m_rib.clear();
  m_vportMap.clear();
  m_statistics = CompactRoutingTable::Statistics();
}

void CompactRoutingTablePrivate::setLandmark(bool landmark)
{
  RecursiveUniqueLock lock(m_mutex);

  bool changed = (landmark != m_landmark);
  m_landmark = landmark;
  
  if (changed) {
    // Perform local address selection
    selectLocalAddress();
  }
}

void CompactRoutingTablePrivate::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
{
  RecursiveUniqueLock lock(m_mutex);

  // Dump vport mappings
  stream << "*** Vport mappings:" << std::endl;
  for (VportMap::const_iterator i = m_vportMap.begin(); i != m_vportMap.end(); ++i) {
    stream << "VPORT[" << i->right << "] = " << i->left.as(NodeIdentifier::Format::Hex) << std::endl;
  }

  // Dump routing table entries for each destination
  stream << "*** RT entries:" << std::endl;
  stream << "Count: " << m_rib.size() << std::endl;
  NodeIdentifier prevId;
  auto &entries = m_rib.get<RIBTags::DestinationId>();
  for (auto i = entries.begin(); i != entries.end(); ++i) {
    RoutingEntryPtr e = *i;
    bool first = (e->destination != prevId);
    if (first) {
      stream << e->destination.as(NodeIdentifier::Format::Hex);
      if (resolve)
        stream << " (" << resolve(e->destination) << ")";
      stream << std::endl;
    }

    // Output type, cost and forward path
    stream << "  " << "t=";
    if (e->landmark)
      stream << "LND ";
    if (e->vicinity)
      stream << "VIC ";
    if (e->extendedVicinity)
      stream << "EVIC";
    stream << " c=" << e->cost << " f-path=";
    for (auto p = e->forwardPath.begin(); p != e->forwardPath.end(); ++p) {
      stream << *p << " ";
    }

    stream << "age=" << e->age().total_seconds() << "s ";

    // Mark currently active route
    if (e->active)
      stream << "*";
    stream << std::endl;

    prevId = e->destination;
  }

  // Dump vicinity size and maximum
  stream << "*** Vicinity:" << std::endl;
  stream << "Current size: " << getCurrentVicinity().size << std::endl;
  stream << "Maximum size: " << getMaximumVicinitySize() << std::endl;

  // Dump number of landmarks
  stream << "*** Landmarks:" << std::endl;
  stream << "Count: " << getLandmarkCount();
  if (m_landmark)
    stream << " (+1 current node)";
  stream << std::endl;
}

void CompactRoutingTablePrivate::dumpTopology(CompactRoutingTable::TopologyDumpGraph &graph,
                                              std::function<std::string(const NodeIdentifier&)> resolve)
{
  RecursiveUniqueLock lock(m_mutex);

  if (!resolve)
    resolve = [&](const NodeIdentifier &nodeId) { return nodeId.hex(); };

  using Tags = CompactRoutingTable::TopologyDumpTags;
  std::string name = resolve(m_localId);
  auto self = graph.add_vertex(name);
  boost::put(boost::get(Tags::NodeName(), graph.graph()), self, name);
  boost::put(boost::get(Tags::NodeGroup(), graph.graph()), self,
    m_sloppyGroup.getGroupPrefix().bin().substr(0, m_sloppyGroup.getGroupPrefixLength()));
  boost::put(boost::get(Tags::NodeIsLandmark(), graph.graph()), self, m_landmark);
  boost::put(boost::get(Tags::NodeStateSize(), graph.graph()), self, m_rib.size());

  auto addEdge = [&](const NodeIdentifier &id, Vport vportId) {
    auto vertex = graph.add_vertex(resolve(id));
    boost::put(boost::get(Tags::NodeName(), graph.graph()), vertex, resolve(id));

    auto edge = boost::add_edge(self, vertex, graph).first;
    boost::put(boost::get(Tags::LinkVportId(), graph.graph()), edge, vportId);
    boost::put(boost::get(Tags::LinkWeight(), graph.graph()), edge, 1);
  };

  auto &entries = m_rib.get<RIBTags::DestinationId>();
  for (auto i = entries.begin(); i != entries.end(); ++i) {
    RoutingEntryPtr e = *i;
    if (!e->isDirect())
      continue;

    addEdge(e->destination, e->originVport());
  }
}

CompactRoutingTable::CompactRoutingTable(Context &context,
                                         const NodeIdentifier &localId,
                                         NetworkSizeEstimator &sizeEstimator,
                                         SloppyGroupManager &sloppyGroup)
  : signalExportEntry(context),
    signalRetractEntry(context),
    signalAddressChanged(context),
    signalLandmarkLearned(context),
    signalLandmarkRemoved(context),
    signalVicinityLearned(context),
    signalVicinityRemoved(context),

    d(new CompactRoutingTablePrivate(context, localId, sizeEstimator, sloppyGroup, *this))
{
}

NodeIdentifier CompactRoutingTable::getActiveRoute(const NodeIdentifier &destination)
{
  return d->getActiveRoute(destination);
}

CompactRoutingTable::SloppyGroupRelay CompactRoutingTable::getSloppyGroupRelay(const NodeIdentifier &destination)
{
  return d->getSloppyGroupRelay(destination);
}

Vport CompactRoutingTable::getVportForNeighbor(const NodeIdentifier &neighbor)
{
  return d->getVportForNeighbor(neighbor);
}

NodeIdentifier CompactRoutingTable::getNeighborForVport(Vport vport) const
{
  return d->getNeighborForVport(vport);
}

bool CompactRoutingTable::import(RoutingEntryPtr entry)
{
  return d->import(entry);
}

bool CompactRoutingTable::retract(const NodeIdentifier &destination)
{
  return d->retract(destination);
}

bool CompactRoutingTable::retract(Vport vport,
                                  const NodeIdentifier &destination)
{
  return d->retract(vport, destination);
}

void CompactRoutingTable::fullUpdate(const NodeIdentifier &peer)
{
  return d->fullUpdate(peer);
}

size_t CompactRoutingTable::size() const
{
  return d->size();
}

size_t CompactRoutingTable::sizeActive() const
{
  return d->sizeActive();
}

size_t CompactRoutingTable::sizeVicinity() const
{
  return d->sizeVicinity();
}

std::list<CompactRoutingTable::VicinityDescriptor> CompactRoutingTable::getVicinity() const
{
  RecursiveUniqueLock lock(d->m_mutex);
  std::list<VicinityDescriptor> vicinity;
  auto entries = d->m_rib.get<RIBTags::VicinityAny>().equal_range(true);
  for (auto it = entries.first; it != entries.second; ++it) {
    vicinity.push_back(VicinityDescriptor{ (*it)->destination, (*it)->hops() });
  }

  return vicinity;
}

void CompactRoutingTable::clear()
{
  return d->clear();
}

void CompactRoutingTable::setLandmark(bool landmark)
{
  return d->setLandmark(landmark);
}

bool CompactRoutingTable::isLandmark() const
{
  return d->m_landmark;
}

LandmarkAddressList CompactRoutingTable::getLocalAddresses() const
{
  RecursiveUniqueLock lock(d->m_mutex);
  return d->m_localAddress;
}

LandmarkAddress CompactRoutingTable::getLocalAddress() const
{
  RecursiveUniqueLock lock(d->m_mutex);
  if (d->m_localAddress.empty())
    return LandmarkAddress();

  return d->m_localAddress.front();
}

const CompactRoutingTable::Statistics &CompactRoutingTable::statistics() const
{
  return d->m_statistics;
}

void CompactRoutingTable::dump(std::ostream &stream,
                               std::function<std::string(const NodeIdentifier&)> resolve) const
{
  return d->dump(stream, resolve);
}

void CompactRoutingTable::dumpTopology(TopologyDumpGraph &graph,
                                       std::function<std::string(const NodeIdentifier&)> resolve)
{
  d->dumpTopology(graph, resolve);
}

}