/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <k@jst.sm>
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
#include "social/address.h"

namespace UniSphere {

LandmarkAddress::LandmarkAddress()
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

LandmarkAddress::LandmarkAddress(const NodeIdentifier &landmarkId, const google::protobuf::RepeatedField<google::protobuf::uint32> &path)
  : m_landmarkId(landmarkId)
{
  for (auto it = path.begin(); it != path.end(); ++it) {
    m_path.push_back(*it);
  }
}

bool LandmarkAddress::operator==(const LandmarkAddress &other) const
{
  return m_landmarkId == other.m_landmarkId && m_path == other.m_path;
}

std::ostream &operator<<(std::ostream &stream, const LandmarkAddress &address)
{
  stream << "[" << address.landmarkId().hex() << ", ";

  if (address.path().empty()) {
    stream << "<>]";
  } else {
    auto it = address.path().begin();
    stream << "<" << *it;
    while (++it != address.path().end())
      stream << "-" << *it;
    stream << ">]";
  }

  return stream;
}

}
