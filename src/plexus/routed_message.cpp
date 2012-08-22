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
#include "plexus/routed_message.h"

namespace UniSphere {

RoutedMessage::RoutedMessage(const Message &msg)
{
  Protocol::Plexus::RoutedMessage pmsg = message_cast<Protocol::Plexus::RoutedMessage>(msg);
  m_sourceNodeId = NodeIdentifier(pmsg.sourcenode(), NodeIdentifier::Format::Raw);
  m_sourceCompId = pmsg.sourcecomp();
  m_destinationKeyId = NodeIdentifier(pmsg.destinationid(), NodeIdentifier::Format::Raw);
  m_destinationCompId = pmsg.destinationcomp();
  m_hopCount = pmsg.hopcount();
  m_payloadType = pmsg.type();
  m_payload = pmsg.message();
}

RoutedMessage::RoutedMessage(const NodeIdentifier &sourceNodeId, uint32_t sourceCompId,
                             const NodeIdentifier &destinationKeyId, uint32_t destinationCompId,
                             std::uint32_t type, const google::protobuf::Message &msg)
  : m_sourceNodeId(sourceNodeId),
    m_sourceCompId(sourceCompId),
    m_destinationKeyId(destinationKeyId),
    m_destinationCompId(destinationCompId),
    m_hopCount(RoutedMessage::default_hop_count),
    m_payloadType(type)
{
  msg.SerializeToString(&m_payload);
}

bool RoutedMessage::isValid() const
{
  return m_sourceNodeId.isValid() && m_destinationKeyId.isValid() && m_hopCount > 0;
}

void RoutedMessage::decrementHopCount()
{
  if (m_hopCount > 0)
    m_hopCount--;
}

Protocol::Plexus::RoutedMessage *RoutedMessage::serialize()
{
  Protocol::Plexus::RoutedMessage *pmsg = new Protocol::Plexus::RoutedMessage;
  pmsg->set_sourcenode(m_sourceNodeId.as(NodeIdentifier::Format::Raw));
  pmsg->set_sourcecomp(m_sourceCompId);
  pmsg->set_destinationid(m_destinationKeyId.as(NodeIdentifier::Format::Raw));
  pmsg->set_destinationcomp(m_destinationCompId);
  pmsg->set_hopcount(m_hopCount);
  pmsg->set_type(m_payloadType);
  pmsg->set_message(m_payload);
  return pmsg;
}
  
}
