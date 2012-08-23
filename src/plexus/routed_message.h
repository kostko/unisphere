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
#ifndef UNISPHERE_PLEXUS_ROUTEDMESSAGE_H
#define UNISPHERE_PLEXUS_ROUTEDMESSAGE_H

#include "core/globals.h"
#include "src/plexus/plexus.pb.h"
#include "interplex/message.h"
#include "identity/node_identifier.h"

namespace UniSphere {
  
/**
 * A message that can be routed over multiple hops.
 */
class UNISPHERE_EXPORT RoutedMessage {
public:
  /// Default hop count for outgoing packets
  static const std::uint8_t default_hop_count = 30;
  
  /**
   * Constructs a routed message based on an existing message received
   * via an Interplex link.
   * 
   * @param msg Existing transport message
   */
  explicit RoutedMessage(const Message &msg);
  
  /**
   * Constructs a new routed message.
   * 
   * @param sourceNodeId Source node identifier
   * @param sourceCompId Source component identifier
   * @param destinationKeyId Destination key identifier
   * @param destinationCompId Destination component identifier
   * @param type Payload message type
   * @param msg Message
   */
  RoutedMessage(const NodeIdentifier &sourceNodeId, std::uint32_t sourceCompId,
                const NodeIdentifier &destinationKeyId, std::uint32_t destinationCompId,
                std::uint32_t type, const google::protobuf::Message &msg);
  
  /**
   * Returns true if the message is considered a valid one. Invalid messages
   * should be dropped by routers.
   */
  bool isValid() const;
  
  /**
   * Decrements the hop count field.
   */
  void decrementHopCount();
  
  /**
   * Returns the source node identifier.
   */
  inline const NodeIdentifier &sourceNodeId() const { return m_sourceNodeId; }
  
  /**
   * Returns the source component identifier.
   */
  inline std::uint32_t sourceCompId() const { return m_sourceCompId; }
  
  /**
   * Returns the destination key identifier.
   */
  inline const NodeIdentifier &destinationKeyId() const { return m_destinationKeyId; }
  
  /**
   * Returns the destination component identifier.
   */
  inline std::uint32_t destinationCompId() const { return m_destinationCompId; }
  
  /**
   * Returns the payload type.
   */
  inline std::uint32_t payloadType() const { return m_payloadType; }
  
  /**
   * Returns the hop count.
   */
  inline std::uint8_t hopCount() const { return m_hopCount; }
  
  /**
   * Serializes the routed message into a Protocol Buffers message.
   */
  Protocol::Plexus::RoutedMessage *serialize() const;
private:
  /// Source node identifier
  NodeIdentifier m_sourceNodeId;
  /// Source component identifier
  std::uint32_t m_sourceCompId;
  /// Destination key identifier
  NodeIdentifier m_destinationKeyId;
  /// Destination component identifier
  std::uint32_t m_destinationCompId;
  
  /// Hop count
  std::uint8_t m_hopCount;
  
  /// Payload type
  std::uint32_t m_payloadType;
  /// Payload data
  std::string m_payload;
};

}

#endif
