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

RoutingEntry::RoutingEntry()
{
}

bool RoutingEntry::operator==(const RoutingEntry &other) const
{
  return destination == other.destination && type == other.type &&
    cost == other.cost && path == other.path;
}

CompactRoutingTable::CompactRoutingTable(NetworkSizeEstimator &sizeEstimator)
  : m_sizeEstimator(sizeEstimator)
{
}

size_t CompactRoutingTable::getMaximumVicinitySize() const
{
  // TODO This is probably not the best way (int -> double -> sqrt -> int)
  double n = static_cast<double>(m_sizeEstimator.getNetworkSize());
  return static_cast<size_t>(std::sqrt(n * std::log(n)));
}

bool CompactRoutingTable::import(const RoutingEntry &entry)
{
  RecursiveUniqueLock lock(m_mutex);

  if (entry.isNull())
    return false;

  // An entry should be inserted if it represents a landmark or if it falls
  // into the vicinity of the current node
  if (entry.type == RoutingEntry::Type::Vicinity) {
    // Verify that it falls into the vicinity
    auto entries = m_rib.get<RIBTags::TypeCost>().equal_range(boost::make_tuple(RoutingEntry::Type::Vicinity));
    if (std::distance(entries.first, entries.second) >= getMaximumVicinitySize()) {
      auto back = --entries.second;
      if ((*back).cost > entry.cost) {
        m_rib.get<RIBTags::TypeCost>().erase(back);
      } else {
        return false;
      }
    }
  }

  m_rib.insert(entry);

  // Importing an entry might cause the best path to destination to change; if it
  // does, we need to export the entry to others as well
  if (*m_rib.get<RIBTags::DestinationId>().find(entry.destination) == entry)
    signalExportEntry(entry);

  return true;
}

void CompactRoutingTable::setLandmark(bool landmark)
{
  // TODO Handle landmark setup/tear down
  m_landmark = landmark;
}

}