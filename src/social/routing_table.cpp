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

namespace UniSphere {

// Define the invalid routing entry instance that can be used for
// returning references to invalid entries
const RoutingEntry RoutingEntry::INVALID = RoutingEntry();

RoutingEntry::RoutingEntry()
{
}

RoutingEntry::RoutingEntry(const NodeIdentifier &destination, Type type)
  : destination(destination),
    type(type)
{
}

bool RoutingEntry::operator==(const RoutingEntry &other) const
{
  return destination == other.destination && type == other.type &&
    cost == other.cost && forwardPath == other.forwardPath && reversePath == other.reversePath;
}

CompactRoutingTable::CompactRoutingTable(NetworkSizeEstimator &sizeEstimator)
  : m_sizeEstimator(sizeEstimator),
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

bool CompactRoutingTable::import(const RoutingEntry &entry)
{
  RecursiveUniqueLock lock(m_mutex);

  if (entry.isNull())
    return false;

  bool needsInsertion = true;

  // Check if an entry to the same destination from the same vport already exists; in
  // this case, the announcement counts as an implicit retract
  auto &ribVport = m_rib.get<RIBTags::VportDestination>();
  auto existing = ribVport.find(boost::make_tuple(entry.originVport(), entry.destination));
  if (existing != ribVport.end()) {
    // Ignore import when the existing entry is the same as the new one
    if (*existing == entry) {
      // Update the entry's last seen timestamp
      ribVport.modify(existing, [&](RoutingEntry &e) {
        e.lastUpdate = boost::posix_time::microsec_clock::universal_time();
      });
      std::cout << "updating existing rt entry: vport=" << entry.originVport() << std::endl;
      return false;
    }

    ribVport.replace(existing, entry);
    needsInsertion = false;
  }

  if (needsInsertion) {
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
          std::cout << "dropping rt entry due to vicinity overflow" << std::endl;
          return false;
        }
      }
    }

    std::cout << "inserting rt entry: vport=" << entry.originVport() << ", type=" << (int) entry.type << ", cost=" << entry.cost << std::endl;
    m_rib.insert(entry);
  }

  // TODO: Update local address(es) based on remote landmark announces

  // Importing an entry might cause the best path to destination to change; if it
  // does, we need to export the entry to others as well
  if (getActiveRoute(entry.destination) == entry)
    signalExportEntry(entry);

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
    ribVport.erase(it);

    if (explicitRetract) {
      if (candidates.first != candidates.second) {
        // No need for an explicit retract as export counts as an implicit one
        signalExportEntry(*candidates.first);
      } else {
        signalRetractEntry(entry);
      }
    }
  }

  return false;
}

void CompactRoutingTable::setLandmark(bool landmark)
{
  // TODO: Handle landmark setup/tear down
  // TODO: Landmarks themselves have null addresses (as all nodes need to have them in RIB)
  m_landmark = landmark;
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