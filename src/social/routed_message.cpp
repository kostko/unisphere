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
#include "social/routed_message.h"

namespace UniSphere {

RoutedMessage::RoutedMessage(const Message &msg)
{
  Protocol::RoutedMessage pmsg = message_cast<Protocol::RoutedMessage>(msg);
  m_sourceAddress = LandmarkAddress(
    NodeIdentifier(pmsg.sourcelandmark(), NodeIdentifier::Format::Raw),
    pmsg.sourceaddress()
  );
  m_sourceNodeId = NodeIdentifier(pmsg.sourcenode(), NodeIdentifier::Format::Raw);
  m_sourceCompId = pmsg.sourcecomp();

  if (pmsg.has_destinationlandmark()) {
    m_destinationAddress = LandmarkAddress(
      NodeIdentifier(pmsg.destinationlandmark(), NodeIdentifier::Format::Raw),
      pmsg.destinationaddress()
    );
  }
  
  m_destinationNodeId = NodeIdentifier(pmsg.destinationnode(), NodeIdentifier::Format::Raw);
  m_destinationCompId = pmsg.destinationcomp();
  m_hopCount = pmsg.hopcount();
  m_deliveryMode = pmsg.delivery();
  m_payloadType = pmsg.type();
  m_payload = pmsg.payload();
  m_originLinkId = msg.originator();
}

RoutedMessage::RoutedMessage(const LandmarkAddress &sourceAddress,
                             const NodeIdentifier &sourceNodeId,
                             uint32_t sourceCompId,
                             const LandmarkAddress &destinationAddress,
                             const NodeIdentifier &destinationNodeId,
                             uint32_t destinationCompId,
                             std::uint32_t type,
                             const google::protobuf::Message &msg,
                             const RoutingOptions &opts)
  : m_sourceAddress(sourceAddress),
    m_sourceNodeId(sourceNodeId),
    m_sourceCompId(sourceCompId),
    m_destinationAddress(destinationAddress),
    m_destinationNodeId(destinationNodeId),
    m_destinationCompId(destinationCompId),
    m_hopCount(opts.hopLimit),
    m_deliveryMode(false),
    m_payloadType(type),
    m_options(opts)
{
  msg.SerializeToString(&m_payload);
}

bool RoutedMessage::isValid() const
{
  return m_sourceNodeId.isValid() && m_destinationNodeId.isValid() && m_hopCount > 0;
}

void RoutedMessage::processHop()
{
  if (m_hopCount > 0)
    m_hopCount--;
}

void RoutedMessage::processSourceRouteHop()
{
  if (m_deliveryMode) {
    // After the message has reached a designated landmark it should be source-routed,
    // so we remove one hop in the address
    m_destinationAddress.shift();
  }
}

void RoutedMessage::serialize(Protocol::RoutedMessage &pmsg) const
{
  pmsg.Clear();
  pmsg.set_sourcelandmark(m_sourceAddress.landmarkId().as(NodeIdentifier::Format::Raw));
  for (Vport port : m_sourceAddress.path())
    pmsg.add_sourceaddress(port);
  pmsg.set_sourcenode(m_sourceNodeId.as(NodeIdentifier::Format::Raw));
  pmsg.set_sourcecomp(m_sourceCompId);
  pmsg.set_destinationlandmark(m_destinationAddress.landmarkId().as(NodeIdentifier::Format::Raw));
  for (Vport port : m_destinationAddress.path())
    pmsg.add_destinationaddress(port);
  pmsg.set_destinationnode(m_destinationNodeId.as(NodeIdentifier::Format::Raw));
  pmsg.set_destinationcomp(m_destinationCompId);
  pmsg.set_hopcount(m_hopCount);
  pmsg.set_delivery(m_deliveryMode);
  pmsg.set_type(m_payloadType);
  pmsg.set_payload(m_payload);
}
  
}
