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
#ifndef UNISPHERE_INTERPLEX_CONTACT_H
#define UNISPHERE_INTERPLEX_CONTACT_H

#include "core/globals.h"
#include "identity/node_identifier.h"
#include "src/interplex/contact.pb.h"

#include <boost/asio.hpp>
#include <boost/any.hpp>
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
  };
  
  /**
   * Constructs a null address.
   */
  Address();
  
  /**
   * Constructs an IP address.
   */
  Address(const boost::asio::ip::tcp::endpoint &endpoint);
  
  /**
   * Constructs an IP address.
   */
  Address(const boost::asio::ip::address &ip, unsigned short port);
  
  /**
   * Constructs an IP address.
   */
  Address(const std::string &ip, unsigned short port);
  
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
  boost::asio::ip::tcp::endpoint toIpEndpoint() const;
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
   * @param nodeId Node identifier
   */
  explicit Contact(const NodeIdentifier &nodeId);
  
  /**
   * Returns true if this contact record is a null one.
   */
  bool isNull() const;
  
  /**
   * Returns the node's identifier.
   */
  NodeIdentifier nodeId() const;
  
  /**
   * Returns the address list.
   */
  const AddressMap &addresses() const;
  
  /**
   * Returns the address list.
   */
  AddressMap &addresses();
  
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
private:
  /// Node identifier
  NodeIdentifier m_nodeId;
  
  /// Contact addresses
  AddressMap m_addresses;
};

}

#endif