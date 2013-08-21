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
#ifndef UNISPHERE_SOCIAL_ROUTEDMESSAGE_H
#define UNISPHERE_SOCIAL_ROUTEDMESSAGE_H

#include "core/globals.h"
#include "src/social/messages.pb.h"
#include "interplex/message.h"
#include "interplex/link.h"
#include "identity/node_identifier.h"
#include "social/address.h"

namespace UniSphere {

/**
 * Routing options that can be attached to outgoing routed messages. These
 * options are only visible within the router and are not encoded in
 * messages.
 */
class UNISPHERE_EXPORT RoutingOptions {
public:
  /**
   * Constructs default options.
   */
  RoutingOptions()
    : directDelivery(false),
      hopLimit(30),
      trackHopDistance(false)
  {};

  /**
   * Sets direct delivery requirement - this means that the local routing decision
   * will never try to handle destination identifier resolution.
   */
  RoutingOptions &setDirectDelivery(bool delivery) { directDelivery = delivery; return *this; }
  
  /**
   * Forces the packet to be delivered over a specific link.
   */
  RoutingOptions &setDeliverVia(const NodeIdentifier &linkId) { deliverVia = Contact(linkId); return *this; }
  
  /**
   * Forces the packet to be delivered over a specific contact.
   */
  RoutingOptions &setDeliverVia(const Contact &contact) { deliverVia = contact; return *this; }

  /**
   * Sets a specific hop limit on the outgoing packet.
   */
  RoutingOptions &setHopLimit(std::uint8_t limit) { hopLimit = limit; return *this; }

  /**
   * Enables or disables hop distance tracking.
   */
  RoutingOptions &setTrackHopDistance(bool track) { trackHopDistance = track; return *this; }
public:
  /// Force delivery over a specific link
  Contact deliverVia;
  /// Force direct delivery
  bool directDelivery;
  /// Set a specific hop limit
  std::uint8_t hopLimit;
  /// Enables hop count measurement
  bool trackHopDistance;
};
  
/**
 * A message that can be routed over multiple hops.
 */
class UNISPHERE_EXPORT RoutedMessage {
public:  
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
   * @param sourceAddress Source node landmark-relative address
   * @param sourceNodeId Source node identifier
   * @param sourceCompId Source component identifier
   * @param destinationAddress Destination node landmark-relative address
   * @param destinationNodeId Destination node identifier
   * @param destinationCompId Destination component identifier
   * @param type Payload message type
   * @param msg Message
   * @param opts Routing options
   */
  RoutedMessage(const LandmarkAddress &sourceAddress,
                const NodeIdentifier &sourceNodeId,
                std::uint32_t sourceCompId,
                const LandmarkAddress &destinationAddress,
                const NodeIdentifier &destinationNodeId,
                std::uint32_t destinationCompId,
                std::uint32_t type,
                const google::protobuf::Message &msg,
                const RoutingOptions &opts = RoutingOptions());
  
  /**
   * Returns true if the message is considered a valid one. Invalid messages
   * should be dropped by routers.
   */
  bool isValid() const;
  
  /**
   * Decrements the hop count.
   */
  void processHop();

  /**
   * Pops the next vport from the destination address.
   */
  void processSourceRouteHop();

  /**
   * Sets the delivery mode flag on this message.
   */
  inline void setDeliveryMode(bool delivery) { m_deliveryMode = delivery; }

  /**
   * Modifies the landmark-relative destination address.
   */
  inline void setDestinationAddress(const LandmarkAddress &address) { m_destinationAddress = address; }

  /**
   * Returns the source landmark-relative address.
   */
  inline const LandmarkAddress &sourceAddress() const { return m_sourceAddress; }
  
  /**
   * Returns the source node identifier.
   */
  inline const NodeIdentifier &sourceNodeId() const { return m_sourceNodeId; }
  
  /**
   * Returns the source component identifier.
   */
  inline std::uint32_t sourceCompId() const { return m_sourceCompId; }
  
  /**
   * Returns the destination landmark-relative address.
   */
  inline const LandmarkAddress &destinationAddress() const { return m_destinationAddress; }

  /**
   * Returns the destination key identifier.
   */
  inline const NodeIdentifier &destinationNodeId() const { return m_destinationNodeId; }
  
  /**
   * Returns the destination component identifier.
   */
  inline std::uint32_t destinationCompId() const { return m_destinationCompId; }
  
  /**
   * Returns the payload type.
   */
  inline std::uint32_t payloadType() const { return m_payloadType; }
  
  /**
   * Returns the payload.
   */
  inline const std::string &payload() const { return m_payload; }
  
  /**
   * Returns the hop limit.
   */
  inline std::uint8_t hopLimit() const { return m_hopLimit; }

  /**
   * Returns the hop distance.
   */
  inline std::uint8_t hopDistance() const { return m_hopDistance; }

  /**
   * Returns the delivery mode.
   */
  inline bool deliveryMode() const { return m_deliveryMode; }
  
  /**
   * Returns the originator link node identifier. When the originator is null, this
   * means that the message has been generated by the local node.
   */
  inline NodeIdentifier originLinkId() const { return m_originLinkId; }
  
  /**
   * Returns the routing options.
   */
  inline const RoutingOptions &options() const { return m_options; }
  
  /**
   * Serializes the routed message into a Protocol Buffers message.
   *
   * @param pmsg Protocol message to serialize into
   */
  void serialize(Protocol::RoutedMessage &pmsg) const;
private:
  /// Source node landmark-relative address
  LandmarkAddress m_sourceAddress;
  /// Source node identifier
  NodeIdentifier m_sourceNodeId;
  /// Source component identifier
  std::uint32_t m_sourceCompId;
  /// Destination node landmark-relative address
  LandmarkAddress m_destinationAddress;
  /// Destination key identifier
  NodeIdentifier m_destinationNodeId;
  /// Destination component identifier
  std::uint32_t m_destinationCompId;
  
  /// Hop limit
  std::uint8_t m_hopLimit;
  /// Hop distance (optional)
  std::uint8_t m_hopDistance;
  /// Delivery mode
  bool m_deliveryMode;
  
  /// Payload type
  std::uint32_t m_payloadType;
  /// Payload data
  std::string m_payload;
  
  /// Originator link node identifier
  NodeIdentifier m_originLinkId;
  /// Routing options
  RoutingOptions m_options;
};

/**
 * Casts a routed message to a Protocol Buffers message type. The
 * template argument must be a valid protobuf message class.
 *
 * @param msg Message to cast
 * @return A Protocol Buffers message
 */
template<typename T>
T message_cast(const RoutedMessage &msg)
{
  T payload;
  if (!payload.ParseFromString(msg.payload()))
    throw MessageCastFailed();
  return payload;
}

}

#endif
