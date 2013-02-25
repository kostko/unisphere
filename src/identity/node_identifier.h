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
#ifndef UNISPHERE_IDENTITY_IDENTIFIERS_H
#define UNISPHERE_IDENTITY_IDENTIFIERS_H

#include <string>
#include <boost/functional/hash.hpp>

#include "core/globals.h"

namespace UniSphere {

/**
 * An identifier that is used for overlay nodes.
 */
class UNISPHERE_EXPORT NodeIdentifier {
public:
  /// Identifier length in bytes
  static const size_t length = 20;
  /// An invalid (default-constructed) node identifier
  static const NodeIdentifier INVALID;
  
  /**
   * Format specifications for dealing with identifiers.
   */
  enum class Format {
    Raw,
    Hex
  };
  
  /**
   * Constructs an invalid null identifier.
   */
  NodeIdentifier();
  
  /**
   * Class constructor.
   *
   * @param identifier Identifier data
   * @param format Format of identifier data
   */
  explicit NodeIdentifier(const std::string &identifier, Format format = Format::Hex);
  
  /**
   * Copy constructor.
   */
  NodeIdentifier(const NodeIdentifier &other);
  
  /**
   * Returns true when the identifier is an empty one.
   */
  bool isNull() const;
  
  /**
   * Returns true when the identifier is valid. For the identifier to be
   * considered valid it must be exactly @ref length bytes long in its
   * raw form. Empty (null) identifiers are therefore invalid.
   */
  bool isValid() const;
  
  /**
   * Returns the identifier representation in the desired format.
   *
   * @param format Desired output format
   * @return Identifier converted to the desired format
   */
  std::string as(Format format) const;

  /**
   * Convenience alias for as(Format::Hex).
   */
  inline std::string hex() const { return as(Format::Hex); }

  /**
   * Convenience alias for as(Format::Raw).
   */
  inline std::string raw() const { return as(Format::Raw); }
  
  /**
   * Comparison operator.
   */
  bool operator==(const NodeIdentifier &other) const;
  
  /**
   * Comparison operator.
   */
  bool operator!=(const NodeIdentifier &other) const;
  
  /**
   * Comparison operator.
   */
  bool operator<(const NodeIdentifier &other) const;
  
  /**
   * Comparison operator.
   */
  bool operator>(const NodeIdentifier &other) const;
  
  /**
   * Comparison operator.
   */
  bool operator<=(const NodeIdentifier &other) const;
  
  /**
   * Comparison operator.
   */
  bool operator>=(const NodeIdentifier &other) const;
  
  /**
   * Assignment operator.
   */
  NodeIdentifier &operator=(const NodeIdentifier &other);

  /**
   * Increment operator.
   */
  NodeIdentifier &operator+=(double x);
  
  /**
   * Computes XOR function between two node identifiers.
   */
  const NodeIdentifier operator^(const NodeIdentifier &other) const;
  
  /**
   * Returns the length of the longest common prefix (in bits) between two
   * identifiers.
   *
   * @param other The other identifier
   * @return Longest common prefix length (in bits)
   */ 
  size_t longestCommonPrefix(const NodeIdentifier &other) const;

  /**
   * Returns a prefix of this identifier.
   *
   * @param bits Prefix length in bits
   * @return Identifier representing a prefix
   */
  NodeIdentifier prefix(size_t bits) const;
  
  /**
   * Hasher implementation for node identifiers.
   */
  friend size_t hash_value(const NodeIdentifier &identifier)
  {
    boost::hash<std::string> hasher;
    return hasher(identifier.m_identifier);
  }
  
  // Ensure that our hash function is also our friend
  friend class std::hash<NodeIdentifier>;
protected:
  /**
   * A helper method for setting up this identifier from differently formatted
   * data.
   *
   * @param identifier Identifier data
   * @param format Format of identifier data
   */ 
  void setIdentifier(const std::string &identifier, Format format);
private:
  /// The actual identifier in raw form
  std::string m_identifier;
};
  
}

namespace std {
  /**
   * STL hash function implementation for UniSphere::NodeIdentifier.
   */
  template<>
  class hash<UniSphere::NodeIdentifier> {
  public:
    size_t operator()(const UniSphere::NodeIdentifier &identifier) const
    {
      std::hash<std::string> hasher;
      return hasher(identifier.m_identifier);
    }
  };
}

#endif
