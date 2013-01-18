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
      auto &ribType = m_rib.get<RIBTags::TypeCost>();
      auto entries = ribType.equal_range(RoutingEntry::Type::Vicinity);
      if (std::distance(entries.first, entries.second) >= getMaximumVicinitySize()) {
        auto back = --entries.second;
        if ((*back).cost > entry.cost) {
          ribType.erase(back);
        } else {
          return false;
        }
      }
    }

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
    const RoutingEntry &entry = *it;
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
  m_landmark = landmark;
}

}