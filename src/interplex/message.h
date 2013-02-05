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
#ifndef UNISPHERE_INTERPLEX_MESSAGE_H
#define UNISPHERE_INTERPLEX_MESSAGE_H

#include "core/globals.h"
#include "identity/node_identifier.h"

#include <google/protobuf/message.h>

namespace UniSphere {

class Link;
UNISPHERE_SHARED_POINTER(Link);

/**
 * Represents a binary message sent via the UNISPHERE transport. It
 * is a typed size-prefixed binary blob. A simple header consists of
 * an 8-bit message type identifier and a 32-bit unsigned integer payload
 * size.
 */
class UNISPHERE_EXPORT Message {
public:
  /// UNISPHERE message header size in octets
  static const size_t header_size = 5;
  
  /**
   * Message types.
   */
  enum class Type : std::uint8_t {
    /* 0x00 - 0x1F RESERVED FOR SYSTEM PROTOCOLS */
    Null_Protocol       = 0x00,
    Interplex_KeepAlive = 0x01,
    Interplex_Measure   = 0x02,
    Interplex_Hello     = 0x03,
    Plexus_Routed       = 0x04,
    Social_Announce     = 0x05,
    Social_Retract      = 0x06,
    Social_Refresh      = 0x07,
    Social_Routed       = 0x08,
    
    /* 0x20 - 0xEF RESERVED FOR FUTURE USE */
    
    /* 0xF0 - 0xFF USER-DEFINED MESSAGES */
    UserMsg1            = 0xF0,
    UserMsg2            = 0xF1,
    UserMsg3            = 0xF2,
    UserMsg4            = 0xF3,
    UserMsg5            = 0xF4,
    UserMsg6            = 0xF5,
    UserMsg7            = 0xF6,
    UserMsg8            = 0xF7,
    UserMsg9            = 0xF8,
    UserMsg10           = 0xF9,
    UserMsg11           = 0xFA,
    UserMsg12           = 0xFB,
    UserMsg13           = 0xFC,
    UserMsg14           = 0xFD,
    UserMsg15           = 0xFE,
    UserMsg16           = 0xFF,
  };
  
  /**
   * Constructs an empty message of Null_Protocol type with
   * buffer space for the header.
   */
  Message();
  
  /**
   * Constructs a new UNISPHERE message from a Protocol Buffers
   * message.
   *
   * @param type Message protocol type
   * @param msg Protocol Buffers message
   */
  Message(Type type, const google::protobuf::Message& msg);
  
  /**
   * Returns the message protocol type.
   */
  Type type() const;
  
  /**
   * Returns a reference to the underlying message buffer.
   */
  std::vector<char> &buffer();
  
  /**
   * Returns a reference to the underlying message buffer.
   */
  std::vector<char> &buffer() const;
  
  /**
   * Releases ownership of the buffer and allocates a new empty buffer.
   */
  void detach();
  
  /**
   * Sets up the originator identifier for this message.
   *
   * @param nodeId Originator identifier
   */
  void setOriginator(const NodeIdentifier &nodeId);
  
  /**
   * Returns the originator link identifier.
   */
  inline NodeIdentifier originator() const { return m_originator; }
  
  /**
   * Parses message header contained in the message buffer and returns
   * the payload size. This will also set message protocol type and resize
   * the buffer as needed.
   */
  size_t parseHeader();
private:
  /// Message protocol type
  Type m_type;
  
  /// Buffer that holds the payload
  boost::shared_ptr<std::vector<char> > m_buffer;
  
  /// Identifier of the message originator
  NodeIdentifier m_originator;
};

/**
 * Casts an UNISPHERE message to a Protocol Buffers message type. The
 * template argument must be a valid protobuf message class.
 *
 * @param msg Message to cast
 * @return A Protocol Buffers message
 */
template<typename T>
T message_cast(const Message &msg)
{
  T payload;
  payload.ParseFromArray(&msg.buffer()[Message::header_size],
    msg.buffer().size() - Message::header_size);
  return payload;
}

/**
 * Casts raw data to a Protocol Buffers message type. The
 * template argument must be a valid protobuf message class.
 *
 * @param msg Message to cast
 * @return A Protocol Buffers message
 */
template<typename T>
T message_cast(const std::string &data)
{
  T payload;
  payload.ParseFromString(data);
  return payload;
}

}

#endif
