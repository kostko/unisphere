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
#include "interplex/contact.h"
#include "interplex/exceptions.h"

#include <boost/foreach.hpp>

namespace UniSphere {

Address::Address()
  : m_type(Type::Null)
{
}

Address::Address(const boost::asio::ip::tcp::endpoint &endpoint)
  : m_type(Type::IP),
    m_address(endpoint)
{
}

Address::Address(const boost::asio::ip::address &ip, unsigned short port)
  : m_type(Type::IP),
    m_address(boost::asio::ip::tcp::endpoint(ip, port))
{
}

Address::Address(const std::string &ip, unsigned short port)
  : m_type(Type::IP),
    m_address(boost::asio::ip::tcp::endpoint(
      boost::asio::ip::address::from_string(ip), port
    ))
{
}

bool Address::operator==(const Address &other) const
{
  if (m_type != other.m_type)
    return false;
  
  switch (m_type) {
    case Type::IP: return toIpEndpoint() == other.toIpEndpoint();
    default: return false;
  }
}

bool Address::operator<(const Address &other) const
{
  if (m_type != other.m_type)
    return false;
  
  switch (m_type) {
    case Type::IP: return toIpEndpoint() < other.toIpEndpoint();
    default: return false;
  }
}

boost::asio::ip::tcp::endpoint Address::toIpEndpoint() const
{
  if (m_type != Type::IP)
    throw AddressTypeMismatch();
  
  return boost::any_cast<boost::asio::ip::tcp::endpoint>(m_address);
}

Contact::Contact()
  : m_nodeId()
{
}

Contact::Contact(const NodeIdentifier &nodeId)
  : m_nodeId(nodeId)
{
}

bool Contact::isNull() const
{
  return m_nodeId.isNull();
}

NodeIdentifier Contact::nodeId() const
{
  return m_nodeId;
}

const AddressMap &Contact::addresses() const
{
  return m_addresses;
}

AddressMap &Contact::addresses()
{
  return m_addresses;
}
  
void Contact::addAddress(const Address &address, int priority)
{
  m_addresses.insert(std::pair<int, Address>(priority, address));
}

void Contact::setPriority(AddressMap::iterator address, int priority)
{
  Address addr = (*address).second;
  m_addresses.erase(address);
  addAddress(addr, priority);
}

Protocol::Contact Contact::toMessage() const
{
  Protocol::Contact result;
  result.set_node_id(m_nodeId.as(NodeIdentifier::Format::Raw));
  
  typedef std::pair<int, Address> AddressPair;
  BOOST_FOREACH(const AddressPair &p, m_addresses) {
    // Only addresses of type 'IP' can be represented as protocol messages
    if (p.second.type() == Address::Type::IP) {
      Protocol::Address *addr = result.add_addresses();
      addr->set_address(p.second.toIpEndpoint().address().to_string());
      addr->set_port(p.second.toIpEndpoint().port());
    }
  }
  
  return result;
}

Contact Contact::fromMessage(const Protocol::Contact &msg)
{
  Contact result(NodeIdentifier(msg.node_id(), NodeIdentifier::Format::Raw));
  BOOST_FOREACH(const Protocol::Address &addr, msg.addresses()) {
    result.addAddress(Address(addr.address(), addr.port()));
  }
  
  return result;
}

}
