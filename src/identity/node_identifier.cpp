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
#include "identity/node_identifier.h"

#include <bitset>
#include <gmpxx.h>
#include <botan/botan.h>

namespace UniSphere {

// Define the invalid node identifier instance that can be used for
// returning references to invalid identifiers
const NodeIdentifier NodeIdentifier::INVALID = NodeIdentifier();

NodeIdentifier::NodeIdentifier()
{
}

NodeIdentifier::NodeIdentifier(const std::string &identifier, Format format)
{
  setIdentifier(identifier, format);
}
  
NodeIdentifier::NodeIdentifier(const NodeIdentifier &other)
{
  m_identifier = other.m_identifier;
}

NodeIdentifier NodeIdentifier::random()
{
  Botan::AutoSeeded_RNG rng;
  char nodeId[NodeIdentifier::length];
  rng.randomize((Botan::byte*) &nodeId, sizeof(nodeId));
  return NodeIdentifier(std::string(nodeId, sizeof(nodeId)), NodeIdentifier::Format::Raw);
}

NodeIdentifier &NodeIdentifier::operator=(const NodeIdentifier &other)
{
  m_identifier = other.m_identifier;
  return *this;
}

bool NodeIdentifier::isNull() const
{
  return m_identifier.size() == 0;
}

void NodeIdentifier::setIdentifier(const std::string &identifier, Format format)
{
  switch (format) {
    // Raw format does not need any conversion
    case Format::Raw: m_identifier = identifier; break;
    
    // We need to convert from hexadecimal format to binary format
    case Format::Hex: {
      try {
        Botan::Pipe pipe(new Botan::Hex_Decoder);
        pipe.process_msg(identifier);
        m_identifier = pipe.read_all_as_string(0);
      } catch (std::exception &e) {
        m_identifier.clear();
      }
      break;
    }
  }
  
  if (!isValid())
    m_identifier.clear();
}

bool NodeIdentifier::isValid() const
{
  return m_identifier.size() == NodeIdentifier::length;
}

std::string NodeIdentifier::as(Format format) const
{
  if (!isValid())
    return "";
  
  switch (format) {
    // Raw format does not need any conversion
    case Format::Raw: return m_identifier;
    
    // We need to convert to hexadecimal format
    case Format::Hex: {
      Botan::Pipe pipe(new Botan::Hex_Encoder(Botan::Hex_Encoder::Lowercase));
      pipe.process_msg(m_identifier);
      return pipe.read_all_as_string(0);
    }
  }
  
  return "";
}

bool NodeIdentifier::operator==(const NodeIdentifier &other) const
{
  return m_identifier == other.m_identifier;
}

bool NodeIdentifier::operator!=(const NodeIdentifier &other) const
{
  return m_identifier != other.m_identifier;
}

bool NodeIdentifier::operator<(const NodeIdentifier &other) const
{
  return m_identifier < other.m_identifier;
}

bool NodeIdentifier::operator>(const NodeIdentifier &other) const
{
  return m_identifier > other.m_identifier;
}

bool NodeIdentifier::operator<=(const NodeIdentifier &other) const
{
  return m_identifier <= other.m_identifier;
}

bool NodeIdentifier::operator>=(const NodeIdentifier &other) const
{
  return m_identifier >= other.m_identifier;
}

const NodeIdentifier NodeIdentifier::operator^(const NodeIdentifier &other) const
{
  NodeIdentifier result;
  
  // In case of invalid identifiers return an invalid null identifier
  if (!isValid() || !other.isValid()) {
    return result;
  } else {
    result.m_identifier.resize(NodeIdentifier::length);
  }
  
  std::string::const_iterator it = m_identifier.begin();
  std::string::const_iterator otherIt = other.m_identifier.begin();
  std::string::iterator resultIt = result.m_identifier.begin();
  
  // Compute XOR function byte by byte
  for (; it != m_identifier.end(); ++it, ++otherIt, ++resultIt) {
    *resultIt = *it ^ *otherIt;
  }
  
  return result;
}

NodeIdentifier &NodeIdentifier::operator+=(double x)
{
  if (!isValid())
    return *this;

  mpz_class id;
  id.set_str(hex(), 16);
  id += x;

  // Pad resulting string with zeroes to achieve proper size
  std::string result = id.get_str(16);
  result.insert(0, 2*NodeIdentifier::length - result.size(), '0');

  setIdentifier(result, Format::Hex);
  return *this;
}

NodeIdentifier NodeIdentifier::distanceTo(const NodeIdentifier &other) const
{
  NodeIdentifier distance;
  if (!isValid() || !other.isValid())
    return distance;

  mpz_class a, b, c;
  a.set_str(hex(), 16);
  b.set_str(other.hex(), 16);
  c = (a > b) ? a - b : b - a;

  // Pad resulting string with zeroes to achieve proper size
  std::string result = c.get_str(16);
  result.insert(0, 2*NodeIdentifier::length - result.size(), '0');

  distance.setIdentifier(result, Format::Hex);
  return distance;
}

double NodeIdentifier::distanceToAsDouble(const NodeIdentifier &other) const
{
  if (!isValid() || !other.isValid())
    return std::numeric_limits<double>::quiet_NaN();

  mpz_class a, b, c;
  a.set_str(hex(), 16);
  b.set_str(other.hex(), 16);
  c = (a > b) ? a - b : b - a;

  return c.get_d();
}

size_t NodeIdentifier::longestCommonPrefix(const NodeIdentifier &other) const
{
  // In case of invalid identifiers return zero
  if (!isValid() || !other.isValid())
    return 0;
  
  size_t lcp = 0;
  bool found_diff = false;
  std::string::const_iterator it = m_identifier.begin();
  std::string::const_iterator otherIt = other.m_identifier.begin();
  
  // Find the first 1 bit in XOR between identifiers
  for (; it != m_identifier.end() && !found_diff; ++it, ++otherIt) {
    std::bitset<8> xored(static_cast<std::uint8_t>(*it ^ *otherIt));
    for (int i = 7; i >= 0; i--) {
      if (xored[i]) {
        found_diff = true;
        break;
      }
      
      lcp++;
    }
  }
  
  return lcp;
}

NodeIdentifier NodeIdentifier::prefix(size_t bits, unsigned char fill) const
{
  NodeIdentifier prefix;
  if (!isValid())
    return prefix;

  // Resize destination identifier
  prefix.m_identifier.resize(NodeIdentifier::length, fill);

  // First copy the first few complete bytes
  std::copy(m_identifier.begin(), m_identifier.begin() + bits / 8, prefix.m_identifier.begin());

  // Copy the remaining bits
  if (bits % 8 != 0) {
    std::uint8_t mask = 0;
    for (int i = 0; i < bits % 8; i++)
      mask |= (1 << (7 - i));

    prefix.m_identifier[bits / 8] &= ~mask;
    prefix.m_identifier[bits / 8] |= m_identifier[bits / 8] & mask;
  }

  return prefix;
}

}
