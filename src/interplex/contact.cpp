/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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

Address::Address(const boost::asio::ip::udp::endpoint &endpoint)
  : Address(boost::asio::ip::tcp::endpoint(endpoint.address(), endpoint.port()))
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

Address::Address(const boost::filesystem::path &path)
  : m_type(Type::Local),
    m_address(boost::asio::local::stream_protocol::endpoint(
      path.string()
    ))
{
}

Address::Address(const boost::asio::local::stream_protocol::endpoint &endpoint)
  : m_type(Type::Local),
    m_address(endpoint)
{
}

bool Address::operator==(const Address &other) const
{
  if (m_type != other.m_type)
    return false;

  switch (m_type) {
    case Type::IP: return toTcpIpEndpoint() == other.toTcpIpEndpoint();
    case Type::Local: return toLocalEndpoint() == other.toLocalEndpoint();
    default: return false;
  }
}

bool Address::operator<(const Address &other) const
{
  if (m_type != other.m_type)
    return false;

  switch (m_type) {
    case Type::IP: return toTcpIpEndpoint() < other.toTcpIpEndpoint();
    case Type::Local: return toLocalEndpoint() < other.toLocalEndpoint();
    default: return false;
  }
}

boost::asio::ip::tcp::endpoint Address::toTcpIpEndpoint() const
{
  if (m_type != Type::IP)
    throw AddressTypeMismatch();

  return boost::any_cast<boost::asio::ip::tcp::endpoint>(m_address);
}

boost::asio::ip::udp::endpoint Address::toUdpIpEndpoint() const
{
  const auto &ep = toTcpIpEndpoint();
  return boost::asio::ip::udp::endpoint(ep.address(), ep.port());
}

boost::asio::local::stream_protocol::endpoint Address::toLocalEndpoint() const
{
  if (m_type != Type::Local)
    throw AddressTypeMismatch();

  return boost::any_cast<boost::asio::local::stream_protocol::endpoint>(m_address);
}

Contact::Contact()
{
}

Contact::Contact(const PublicPeerKey &peerKey)
  : m_peerKey(peerKey)
{
}

bool Contact::isNull() const
{
  return m_peerKey.isNull();
}

NodeIdentifier Contact::nodeId() const
{
  return m_peerKey.nodeId();
}

PublicPeerKey Contact::peerKey() const
{
  return m_peerKey;
}

bool Contact::hasAddresses() const
{
  return !m_addresses.empty();
}

const AddressMap &Contact::addresses() const
{
  return m_addresses;
}

AddressMap &Contact::addresses()
{
  return m_addresses;
}

const Address &Contact::address() const
{
  return m_addresses.begin()->second;
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
  result.set_peer_key(m_peerKey.raw());

  for (const auto &p : m_addresses) {
    // Only addresses of type 'IP' can be represented as protocol messages
    if (p.second.type() == Address::Type::IP) {
      Protocol::Address *addr = result.add_addresses();
      const auto &ep = p.second.toTcpIpEndpoint();
      addr->set_address(ep.address().to_string());
      addr->set_port(ep.port());
    }
  }

  return result;
}

Contact Contact::fromMessage(const Protocol::Contact &msg)
{
  Contact result(PublicPeerKey(msg.peer_key()));
  for (const Protocol::Address &addr : msg.addresses()) {
    result.addAddress(Address(addr.address(), addr.port()));
  }

  return result;
}

bool Contact::operator==(const Contact &other) const
{
  return m_peerKey == other.m_peerKey;
}

}
