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
#ifndef UNISPHERE_INTERPLEX_CONTACT_H
#define UNISPHERE_INTERPLEX_CONTACT_H

#include "core/globals.h"
#include "identity/node_identifier.h"
#include "identity/peer_key.h"
#include "src/interplex/contact.pb.h"

#include <boost/asio.hpp>
#include <boost/any.hpp>
#include <boost/filesystem.hpp>
#include <map>

namespace UniSphere {

/**
 * Node contact address.
 */
class UNISPHERE_EXPORT Address {
public:
  /**
   * Valid address types.
   */
  enum class Type {
    Null,
    IP,
    Local,
  };

  /**
   * Constructs a null address.
   */
  Address();

  /**
   * Constructs an IP address.
   */
  explicit Address(const boost::asio::ip::tcp::endpoint &endpoint);

  /**
   * Constructs an IP address.
   */
  explicit Address(const boost::asio::ip::udp::endpoint &endpoint);

  /**
   * Constructs an IP address.
   */
  Address(const boost::asio::ip::address &ip, unsigned short port);

  /**
   * Constructs an IP address.
   */
  Address(const std::string &ip, unsigned short port);

  /**
   * Constructs a Local socket address.
   */
  explicit Address(const boost::filesystem::path &path);

  /**
   * Constructs a Local socket address.
   */
  explicit Address(const boost::asio::local::stream_protocol::endpoint &endpoint);

  /**
   * Returns true if this address is equal to another.
   */
  bool operator==(const Address &other) const;

  /**
   * Returns true if this address is less than another.
   */
  bool operator<(const Address &other) const;

  /**
   * Returns true if the address is a null one.
   */
  inline bool isNull() const { return m_type == Type::Null; }

  /**
   * Returns address type.
   */
  inline Type type() const { return m_type; }

  /**
   * Returns an IP address representation of this node address. It
   * is an error to call this method when address type is not IP. An
   * exception will be thrown in this case.
   *
   * @throws AddressTypeMismatch
   */
  boost::asio::ip::tcp::endpoint toTcpIpEndpoint() const;

  /**
   * Returns an IP address representation of this node address. It
   * is an error to call this method when address type is not IP. An
   * exception will be thrown in this case.
   *
   * @throws AddressTypeMismatch
   */
  boost::asio::ip::udp::endpoint toUdpIpEndpoint() const;

  /**
   * Returns the local path endpoint representation of this node
   * address. It is an error to call this method when address type is not
   * Local. An exception will be thrown in this case.
   *
   * @throws AddressTypeMismatch
   */
  boost::asio::local::stream_protocol::endpoint toLocalEndpoint() const;
private:
  /// Address type
  Type m_type;
  /// The actual address container
  boost::any m_address;
};

/// Address map
typedef std::multimap<int, Address> AddressMap;

/**
 * Represents node contact information - all known node's addresses
 * are held by these classes.
 */
class UNISPHERE_EXPORT Contact {
public:
  /**
   * Constructs a null contact.
   */
  Contact();

  /**
   * Constructs a new node contact.
   *
   * @param peerKey Peer key
   */
  explicit Contact(const PublicPeerKey &peerKey);

  /**
   * Returns true if this contact record is a null one.
   */
  bool isNull() const;

  /**
   * Returns the node's identifier.
   */
  NodeIdentifier nodeId() const;

  /**
   * Returns the peer key.
   */
  PublicPeerKey peerKey() const;

  /**
   * Returns true when the contact contains some addresses.
   */
  bool hasAddresses() const;

  /**
   * Returns the address list.
   */
  const AddressMap &addresses() const;

  /**
   * Returns the address list.
   */
  AddressMap &addresses();

  /**
   * Returns the first address.
   */
  const Address &address() const;

  /**
   * Adds a new address to this contact record.
   *
   * @param address Address to add
   * @param priority Optional priority (lower value means higher priority)
   */
  void addAddress(const Address &address, int priority = 10);

  /**
   * Changes the priority of an existing contact address.
   *
   * @param address Address iterator
   * @param priority New priority
   */
  void setPriority(AddressMap::iterator address, int priority);

  /**
   * Returns this contact as a protocol buffers message ready for
   * serialization.
   */
  Protocol::Contact toMessage() const;

  /**
   * Returns a contact obtained from a protocol buffers message.
   *
   * @param msg Protocol contact message
   * @return A valid contact
   */
  static Contact fromMessage(const Protocol::Contact &msg);

  /**
   * Comparison operator, only compares node identifiers.
   */
  bool operator==(const Contact &other) const;

  // Ensure that our hash function is also our friend
  friend class std::hash<Contact>;
private:
  /// Peer key
  PublicPeerKey m_peerKey;
  /// Contact addresses
  AddressMap m_addresses;
};

}

namespace std {
  /**
   * STL hash function implementation for UniSphere::Contact.
   */
  template<>
  class hash<UniSphere::Contact> {
  public:
    size_t operator()(const UniSphere::Contact &contact) const
    {
      std::hash<UniSphere::NodeIdentifier> hasher;
      return hasher(contact.m_peerKey.nodeId());
    }
  };
}

#endif
