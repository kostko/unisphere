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
#ifndef UNISPHERE_SOCIAL_ADDRESS_H
#define UNISPHERE_SOCIAL_ADDRESS_H

#include <google/protobuf/repeated_field.h>
#include <list>

#include "identity/node_identifier.h"

namespace UniSphere {

/// Vport identifier type
typedef std::uint32_t Vport;

/// The routing path type that contains a list of vports to reach a destination
typedef std::list<Vport> RoutingPath;

/**
 * Represents a landmark-relative address of the current node. Such an address
 * can be used by other nodes to route messages towards this node.
 */
class UNISPHERE_EXPORT LandmarkAddress {
public:
  /**
   * Constructs a null landmark address.
   */
  LandmarkAddress();

  /**
   * Constructs a landmark address with an empty routing path. Such an address
   * designates the landmark itself.
   *
   * @param landmarkId Landmark identifier
   */
  explicit LandmarkAddress(const NodeIdentifier &landmarkId);

  /**
   * Constructs a landmark address.
   *
   * @param landmarkId Landmark identifier
   * @param path Reverse routing path (from landmark to node)
   */
  LandmarkAddress(const NodeIdentifier &landmarkId, const RoutingPath &path);

  /**
   * Constructs a landmark address.
   *
   * @param landmarkId Landmark identifier
   * @param path Reverse routing path (from landmark to node)
   */
  LandmarkAddress(const NodeIdentifier &landmarkId, const google::protobuf::RepeatedField<google::protobuf::uint32> &path);

  /**
   * Returns true if this landmark-relative address is null.
   */
  inline bool isNull() const { return m_landmarkId.isNull(); }

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

  /**
   * Moves the address one hop forward by removing the first element in
   * the reverse routing path.
   */
  inline void hop() { m_path.pop_front(); }

  /**
   * Compares two landmark addresses for equality.
   */
  bool operator==(const LandmarkAddress &other) const;
private:
  /// Landmark identifier
  NodeIdentifier m_landmarkId;
  /// Reverse routing path
  RoutingPath m_path;
};

/// Operator for easier display of L-R addresses
UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &stream, const LandmarkAddress &address);

}

#endif
