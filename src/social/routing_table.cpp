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

namespace UniSphere {

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

RoutingEntry::RoutingEntry(boost::asio::io_service &service, const NodeIdentifier &destination,
  Type type, std::uint16_t seqno)
  : destination(destination),
    type(type),
    seqno(seqno),
    active(false),
    expiryTimer(service)
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
  return destination == other.destination && type == other.type && seqno == other.seqno &&
    cost == other.cost && forwardPath == other.forwardPath && reversePath == other.reversePath;
}

CompactRoutingTable::CompactRoutingTable(Context &context, const NodeIdentifier &localId,
  NetworkSizeEstimator &sizeEstimator)
  : m_context(context),
    m_localId(localId),
    m_sizeEstimator(sizeEstimator),
    m_nextVport(0),
    m_landmark(false)
{
}

Vport CompactRoutingTable::getVportForNeighbor(const NodeIdentifier &neighbor)
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

NodeIdentifier CompactRoutingTable::getNeighborForVport(Vport vport) const
{
  VportMap::right_const_iterator i = m_vportMap.right.find(vport);
  if (i == m_vportMap.right.end())
    return NodeIdentifier();

  return (*i).second;
}

size_t CompactRoutingTable::getMaximumVicinitySize() const
{
  // TODO: This is probably not the best way (int -> double -> sqrt -> int)
  double n = static_cast<double>(m_sizeEstimator.getNetworkSize());
  return static_cast<size_t>(std::sqrt(n * std::log(n)));
}

boost::tuple<size_t, RoutingEntryPtr> CompactRoutingTable::getCurrentVicinity() const
{
  auto entries = m_rib.get<RIBTags::TypeDestinationCost>().equal_range(RoutingEntry::Type::Vicinity);
  RoutingEntryPtr maxCostEntry;
  NodeIdentifier lastDestination;
  size_t vicinitySize = 0;

  // Count the number of unique destinations in the vicinity
  for (auto it = entries.first; it != entries.second; ++it) {
    if ((*it)->destination != lastDestination) {
      vicinitySize++;
      if (!maxCostEntry || (*it)->cost > maxCostEntry->cost)
        maxCostEntry = *it;
    }

    lastDestination = (*it)->destination;
  }

  return boost::make_tuple(vicinitySize, maxCostEntry);
}

size_t CompactRoutingTable::getLandmarkCount() const
{
  auto entries = m_rib.get<RIBTags::TypeDestinationCost>().equal_range(RoutingEntry::Type::Landmark);
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

void CompactRoutingTable::entryTimerExpired(const boost::system::error_code &error,
  RoutingEntryPtr entry)
{
  if (error)
    return;

  // Retract the entry from the routing table
  retract(entry->originVport(), entry->destination);
}

void CompactRoutingTable::fullUpdate(const NodeIdentifier &peer)
{
  RecursiveUniqueLock lock(m_mutex);
  NodeIdentifier lastDestination;

  // Export all active routes to the selected peer
  auto entries = m_rib.get<RIBTags::ActiveRoutes>().equal_range(true);
  for (auto it = entries.first; it != entries.second; ++it) {
    if ((*it)->destination == lastDestination)
      continue;

    exportEntry(*it, peer);
    lastDestination = (*it)->destination;
  }
}

bool CompactRoutingTable::import(RoutingEntryPtr entry)
{
  RecursiveUniqueLock lock(m_mutex);

  if (!entry || entry->isNull())
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

  // Check if an entry to the same destination from the same vport already exists; in
  // this case, the announcement counts as an implicit retract
  bool landmarkChangedType = false;
  auto &ribVport = m_rib.get<RIBTags::VportDestination>();
  auto existing = ribVport.find(boost::make_tuple(entry->originVport(), entry->destination));
  if (existing != ribVport.end()) {
    // Ignore import when the existing entry is the same as the new one
    if (**existing == *entry) {
      // Update the entry's last seen timestamp
      ribVport.modify(existing, [&](RoutingEntryPtr e) {
        e->lastUpdate = entry->lastUpdate;

        // Restart expiry timer
        if (e->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry)) > 0) {
          e->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, e));
        }
      });
      return false;
    }

    // Update certain attributes of the routing entry
    ribVport.modify(existing, [&](RoutingEntryPtr e) {
      if (e->type != entry->type)
        landmarkChangedType = true;

      e->forwardPath = entry->forwardPath;
      e->reversePath = entry->reversePath;
      e->type = entry->type;
      e->seqno = entry->seqno;
      e->cost = entry->cost;
      e->lastUpdate = entry->lastUpdate;

      // Restart expiry timer
      if (e->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry)) > 0) {
        e->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, e));
      }
    });
  } else {
    // An entry should be inserted if it represents a landmark or if it falls
    // into the vicinity of the current node
    if (entry->type == RoutingEntry::Type::Vicinity) {
      // Verify that it falls into the vicinity
      size_t vicinitySize;
      RoutingEntryPtr maxCostEntry;
      boost::tie(vicinitySize, maxCostEntry) = getCurrentVicinity();

      if (vicinitySize >= getMaximumVicinitySize()) {
        if (maxCostEntry->cost > entry->cost) {
          // Remove the entry with maximum cost
          retract(maxCostEntry->destination);
        } else {
          return false;
        }
      }
    }

    m_rib.insert(entry);

    // Setup expiry timer for the routing entry
    entry->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry));
    entry->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, entry));
  }

  // Determine what the new best route for this destination is
  RoutingEntryPtr newBest, oldBest;
  boost::tie(boost::tuples::ignore, newBest, oldBest) = selectBestRoute(entry->destination);

  if (newBest == oldBest && entry == newBest && landmarkChangedType) {
    // Landmark type of the currently active route has changed
    if (entry->isLandmark())
      signalLandmarkLearned(entry->destination);
    else
      signalLandmarkRemoved(entry->destination);
  }

  return true;
}

void CompactRoutingTable::exportEntry(RoutingEntryPtr entry, const NodeIdentifier &peer)
{
  RouteOriginatorPtr orig = entry->originator;

  // Update route originator sequence number and liveness
  if (orig->age().total_seconds() > CompactRouter::origin_expiry_time ||
      orig->isNewer(entry->seqno) ||
      (entry->seqno == orig->seqno && entry->cost < orig->smallestCost)) {
    orig->seqno = entry->seqno;
    orig->smallestCost = entry->cost;
  }
  orig->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  // Export the entry
  signalExportEntry(entry, peer);
}

boost::tuple<bool, RoutingEntryPtr, RoutingEntryPtr> CompactRoutingTable::selectBestRoute(const NodeIdentifier &destination)
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

    if (!e->isFeasible())
      continue;

    newBest = it;
    break;
  }

  // If there are no feasible routes, return early
  if (newBest == ribDestination.end()) {
    // TODO: Request a new sequence number from route originator
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
      if (oldBestEntry->isLandmark() && !newBestEntry->isLandmark())
        signalLandmarkRemoved(destination);
      else if (!oldBestEntry->isLandmark() && newBestEntry->isLandmark())
        signalLandmarkLearned(destination);
    } else if (newBestEntry->isLandmark()) {
      // A landmark has become the active route
      signalLandmarkLearned(destination);
    }

    // Export new active route to neighbors
    exportEntry(newBestEntry);
  }

  return boost::make_tuple(true, *newBest, oldBestEntry);
}

RoutingEntryPtr CompactRoutingTable::getActiveRoute(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribActive =  m_rib.get<RIBTags::ActiveRoutes>();
  auto entry = ribActive.find(boost::make_tuple(true, destination));
  if (entry == ribActive.end())
    return RoutingEntryPtr();

  return *entry;
}

RoutingEntryPtr CompactRoutingTable::getLongestPrefixMatch(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  // If the RIB is empty, return a null entry
  if (m_rib.empty())
    return RoutingEntryPtr();

  // Find the entry with longest common prefix
  auto &ribDestination = m_rib.get<RIBTags::DestinationId>();
  auto it = ribDestination.upper_bound(boost::make_tuple(destination));
  if (it == ribDestination.end())
    return *(--it);

  // Check if previous entry has longer common prefix
  if (it != ribDestination.begin()) {
    auto pit = it;
    if ((*(--pit))->destination.longestCommonPrefix(destination) >
        (*it)->destination.longestCommonPrefix(destination))
      return *pit;
  }

  return *it;
}

bool CompactRoutingTable::retract(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribDestination = m_rib.get<RIBTags::DestinationId>();
  auto entries = ribDestination.equal_range(boost::make_tuple(destination));
  if (entries.first == entries.second)
    return false;

  bool wasLandmark = false;

  for (auto it = entries.first; it != entries.second;) {
    RoutingEntryPtr entry = *it;
    if (entry->isLandmark())
      wasLandmark = true;

    // Call the erasure method to ensure that the routing table is updated before any
    // announcements are sent
    it = ribDestination.erase(it);

    // Send retractions for active entries
    if (entry->active)
      signalRetractEntry(entry);
  }

  // If this was a landmark, we have just unlearned it
  if (wasLandmark)
    signalLandmarkRemoved(destination);

  return true;
}

bool CompactRoutingTable::retract(Vport vport, const NodeIdentifier &destination)
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

    // Call the erasure method to ensure that the routing table is updated before any
    // announcements are sent
    it = ribVport.erase(it);

    // If entry was part of an active route, we must determine a new active route for this destination
    if (entry->active) {
      if (!selectBestRoute(entry->destination).get<0>())
        signalRetractEntry(entry);
    }
  }

  return false;
}

void CompactRoutingTable::clear()
{
  RecursiveUniqueLock lock(m_mutex);

  m_rib.clear();
  m_vportMap.clear();
}

void CompactRoutingTable::setLandmark(bool landmark)
{
  // TODO: Handle landmark setup/tear down
  m_landmark = landmark;
}

std::list<LandmarkAddress> CompactRoutingTable::getLocalAddresses(size_t count) const
{
  std::list<LandmarkAddress> addresses;

  if (isLandmark()) {
    // Landmark nodes don't need an explicit address as all the other nodes have them in RIB
    addresses.push_back(LandmarkAddress(m_localId));
  } else {
    // Extract nearest count landmarks from RIB and return reverse paths as addresses
    auto entries = m_rib.get<RIBTags::TypeCost>().equal_range(RoutingEntry::Type::Landmark);
    for (auto it = entries.first; it != entries.second; ++it) {
      if (!addresses.empty() && (*it)->destination == addresses.back().landmarkId())
        continue;

      addresses.push_back(LandmarkAddress((*it)->destination, (*it)->reversePath));

      if (addresses.size() >= count)
        break;
    }
  }

  return addresses;
}

LandmarkAddress CompactRoutingTable::getLocalAddress() const
{
  std::list<LandmarkAddress> addresses = getLocalAddresses(1);
  if (addresses.empty())
    return LandmarkAddress();

  return addresses.front();
}

void CompactRoutingTable::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
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
    switch (e->type) {
      case RoutingEntry::Type::Landmark: stream << "LND"; break;
      case RoutingEntry::Type::Vicinity: stream << "VIC"; break;
      default: stream << "???"; break;
    }
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
  stream << "Current size: " << getCurrentVicinity().get<0>() << std::endl;
  stream << "Maximum size: " << getMaximumVicinitySize() << std::endl;

  // Dump number of landmarks
  stream << "*** Landmarks:" << std::endl;
  stream << "Count: " << getLandmarkCount();
  if (isLandmark())
    stream << " (+1 current node)";
  stream << std::endl;
}

}