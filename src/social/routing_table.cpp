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

// Define the invalid routing entry instance that can be used for
// returning references to invalid entries
const RoutingEntry RoutingEntry::INVALID = RoutingEntry();

RouteOriginator::RouteOriginator(const NodeIdentifier &nodeId)
  : identifier(nodeId),
    seqno(0),
    smallestCost(0xFFFF)
{
}

boost::posix_time::time_duration RouteOriginator::age() const
{
  return boost::posix_time::microsec_clock::universal_time() - lastUpdate;
}

RoutingEntry::RoutingEntry()
{
}

RoutingEntry::RoutingEntry(const NodeIdentifier &destination, Type type, std::uint16_t seqno)
  : destination(destination),
    type(type),
    seqno(seqno)
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
  if ((originator->seqno - seqno) & 0x8000)
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

RoutingEntry::Timers::Timers(boost::asio::io_service &service)
  : expiryTimer(service)
{
}

LandmarkAddress::LandmarkAddress(const NodeIdentifier &landmarkId)
  : m_landmarkId(landmarkId)
{
}

LandmarkAddress::LandmarkAddress(const NodeIdentifier &landmarkId, const RoutingPath &path)
  : m_landmarkId(landmarkId),
    m_path(path)
{
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

boost::tuple<size_t, RoutingEntry> CompactRoutingTable::getCurrentVicinity() const
{
  auto entries = m_rib.get<RIBTags::TypeCost>().equal_range(RoutingEntry::Type::Vicinity);
  RoutingEntry maxCostEntry;
  NodeIdentifier lastDestination;
  size_t vicinitySize = 0;

  // Count the number of unique destinations in the vicinity
  for (auto it = entries.first; it != entries.second; ++it) {
    if (it->destination != lastDestination) {
      vicinitySize++;
      if (maxCostEntry.isNull() || it->cost > maxCostEntry.cost)
        maxCostEntry = *it;
    }

    lastDestination = it->destination;
  }

  return boost::make_tuple(vicinitySize, maxCostEntry);
}

size_t CompactRoutingTable::getLandmarkCount() const
{
  auto entries = m_rib.get<RIBTags::TypeCost>().equal_range(RoutingEntry::Type::Landmark);
  NodeIdentifier lastDestination;
  size_t landmarkCount = 0;

  // Count the number of unique destinations in the landmark set
  for (auto it = entries.first; it != entries.second; ++it) {
    if (it->destination != lastDestination)
      landmarkCount++;

    lastDestination = it->destination;
  }

  return landmarkCount;
}

void CompactRoutingTable::entryTimerExpired(const boost::system::error_code &error,
  const RoutingEntry &entry)
{
  if (error)
    return;

  // Retract the entry from the routing table
  retract(entry.originVport(), entry.destination);
}

void CompactRoutingTable::fullUpdate(const NodeIdentifier &peer)
{
  auto &entries = m_rib.get<RIBTags::DestinationId>();
  NodeIdentifier lastDestination;

  // Export all active routes to the selected peer
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    if (it->destination == lastDestination)
      continue;

    signalExportEntry(*it, peer);
    lastDestination = it->destination;
  }
}

bool CompactRoutingTable::import(const RoutingEntry &entry)
{
  RecursiveUniqueLock lock(m_mutex);

  if (entry.isNull())
    return false;

  if (!entry.originator) {
    // Discover the originator for this entry
    auto it = m_originatorMap.find(entry.destination);
    if (it == m_originatorMap.end()) {
      RouteOriginatorPtr nro(new RouteOriginator(entry.destination));
      entry.originator = nro;
      m_originatorMap.insert({{entry.destination, nro}});
    } else {
      entry.originator = it->second;
    }
  }

  // Ensure that this route is feasible before importing it
  if (!entry.isFeasible()) {
    // TODO: Request new sequence number from the originator
    return false;
  }

  // Update route originator sequence number and liveness
  entry.originator->seqno = entry.seqno;
  entry.originator->smallestCost = entry.cost;
  entry.originator->lastUpdate = entry.lastUpdate;

  // Check if an entry to the same destination from the same vport already exists; in
  // this case, the announcement counts as an implicit retract
  auto &ribVport = m_rib.get<RIBTags::VportDestination>();
  auto existing = ribVport.find(boost::make_tuple(entry.originVport(), entry.destination));
  if (existing != ribVport.end()) {
    // Ignore import when the existing entry is the same as the new one
    if (*existing == entry) {
      // Update the entry's last seen timestamp
      ribVport.modify(existing, [&](RoutingEntry &e) {
        e.lastUpdate = entry.lastUpdate;

        // Restart expiry timer
        if (e.timers->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry)) > 0) {
          e.timers->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, boost::cref(e)));
        }
      });
      return false;
    }

    // Update certain attributes of the routing entry
    ribVport.modify(existing, [&](RoutingEntry &e) {
      e.forwardPath = entry.forwardPath;
      e.reversePath = entry.reversePath;
      e.type = entry.type;
      e.seqno = entry.seqno;
      e.cost = entry.cost;
      e.lastUpdate = entry.lastUpdate;

      // Restart expiry timer
      if (e.timers->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry)) > 0) {
        e.timers->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, boost::cref(e)));
      }
    });
  } else {
    // An entry should be inserted if it represents a landmark or if it falls
    // into the vicinity of the current node
    if (entry.type == RoutingEntry::Type::Vicinity) {
      // Verify that it falls into the vicinity
      size_t vicinitySize;
      RoutingEntry maxCostEntry;
      boost::tie(vicinitySize, maxCostEntry) = getCurrentVicinity();

      if (vicinitySize >= getMaximumVicinitySize()) {
        if (maxCostEntry.cost > entry.cost) {
          // Remove the entry with maximum cost
          retract(maxCostEntry.destination);
        } else {
          return false;
        }
      }
    }

    auto rentry = m_rib.insert(entry);

    // Setup timers for the routing entry
    const RoutingEntry &eref = *rentry.first;
    eref.timers = boost::shared_ptr<RoutingEntry::Timers>(new RoutingEntry::Timers(m_context.service()));
    eref.timers->expiryTimer.expires_from_now(boost::posix_time::seconds(CompactRouter::interval_neighbor_expiry));
    eref.timers->expiryTimer.async_wait(boost::bind(&CompactRoutingTable::entryTimerExpired, this, _1, boost::cref(eref)));
  }

  // Importing an entry might cause the best path to destination to change; if it
  // does, we need to export the entry to others as well
  if (getActiveRoute(entry.destination) == entry)
    signalExportEntry(entry, NodeIdentifier::INVALID);

  return true;
}

const RoutingEntry &CompactRoutingTable::getActiveRoute(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto entry = m_rib.get<RIBTags::DestinationId>().find(destination);
  if (entry == m_rib.end())
    return RoutingEntry::INVALID;

  return *entry;
}

bool CompactRoutingTable::retract(const NodeIdentifier &destination)
{
  RecursiveUniqueLock lock(m_mutex);

  auto &ribDestination = m_rib.get<RIBTags::DestinationId>();
  auto entries = ribDestination.equal_range(boost::make_tuple(destination));
  if (entries.first == entries.second)
    return false;

  for (auto it = entries.first; it != entries.second;) {
    const RoutingEntry entry = *it;

    // Call the erasure method to ensure that the routing table is updated before any
    // announcements are sent
    ribDestination.erase(it);

    // Send retractions
    signalRetractEntry(entry);
  }

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
    const RoutingEntry entry = *it;
    auto candidates = m_rib.get<RIBTags::DestinationId>().equal_range(boost::make_tuple(entry.destination));
    bool explicitRetract = false;
    if (*candidates.first == entry) {
      // The entry that is going to be erased is currently part of an active route
      // as it is the top-level entry for the given destination
      explicitRetract = true;
    }

    candidates.first++;
    // Call the erasure method to ensure that the routing table is updated before any
    // announcements are sent
    it = ribVport.erase(it);

    if (explicitRetract) {
      if (candidates.first != candidates.second) {
        // No need for an explicit retract as export counts as an implicit one
        signalExportEntry(*candidates.first, NodeIdentifier::INVALID);
      } else {
        signalRetractEntry(entry);
      }
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
  // TODO: Landmarks themselves have null addresses (as all nodes need to have them in RIB)
  m_landmark = landmark;
}

std::list<LandmarkAddress> CompactRoutingTable::getLocalAddresses(size_t count)
{
  std::list<LandmarkAddress> addresses;

  if (isLandmark()) {
    // Landmark nodes don't need an explicit address as all the other nodes have them in RIB
    addresses.push_back(LandmarkAddress(m_localId));
  } else {
    // Extract nearest count landmarks from RIB and return reverse paths as addresses
    auto entries = m_rib.get<RIBTags::TypeCost>().equal_range(RoutingEntry::Type::Landmark);
    for (auto it = entries.first; it != entries.second; ++it) {
      if (!addresses.empty() && it->destination == addresses.back().landmarkId())
        continue;

      addresses.push_back(LandmarkAddress(it->destination, it->reversePath));

      if (addresses.size() >= count)
        break;
    }
  }

  return addresses;
}

void CompactRoutingTable::dump(std::ostream &stream)
{
  RecursiveUniqueLock lock(m_mutex);

  // Dump vport mappings
  stream << "*** Vport mappings:" << std::endl;
  for (VportMap::const_iterator i = m_vportMap.begin(); i != m_vportMap.end(); ++i) {
    stream << "VPORT[" << i->right << "] = " << i->left.as(NodeIdentifier::Format::Hex) << std::endl;
  }

  // Dump routing table entries for each destination
  stream << "*** RT entries:" << std::endl;
  NodeIdentifier prevId;
  auto &entries = m_rib.get<RIBTags::DestinationId>();
  for (auto i = entries.begin(); i != entries.end(); ++i) {
    bool first = (i->destination != prevId);
    if (first)
      stream << i->destination.as(NodeIdentifier::Format::Hex) << std::endl;

    // Output type, cost and forward path
    stream << "  " << "t=";
    switch (i->type) {
      case RoutingEntry::Type::Landmark: stream << "LND"; break;
      case RoutingEntry::Type::Vicinity: stream << "VIC"; break;
      default: stream << "???"; break;
    }
    stream << " c=" << i->cost << " f-path=";
    for (auto p = i->forwardPath.begin(); p != i->forwardPath.end(); ++p) {
      stream << *p << " ";
    }

    stream << "age=" << i->age().total_seconds() << "s ";

    // Mark currently active route
    if (first)
      stream << "*";
    stream << std::endl;

    prevId = i->destination;
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