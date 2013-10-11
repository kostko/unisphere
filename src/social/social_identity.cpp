/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <jernej@kos.mx>
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
#include "social/social_identity.h"

namespace UniSphere {

SocialIdentity::SocialIdentity(const SocialIdentity &identity)
  : m_localId(identity.m_localId),
    m_peers(identity.m_peers)
{
}

SocialIdentity::SocialIdentity(const NodeIdentifier &localId)
  : m_localId(localId)
{
}

bool SocialIdentity::isPeer(const Contact &contact) const
{
  return m_peers.find(contact.nodeId()) != m_peers.end();
}

void SocialIdentity::addPeer(const Contact &peer)
{
  if (peer.isNull())
    return;

  m_peers[peer.nodeId()] = peer;
  signalPeerAdded(peer);
}

void SocialIdentity::removePeer(const NodeIdentifier &nodeId)
{
  m_peers.erase(nodeId);
  signalPeerRemoved(nodeId);
}

Contact SocialIdentity::getPeerContact(const NodeIdentifier &nodeId) const
{
  if (!nodeId.isValid())
    return Contact();
  
  auto it = m_peers.find(nodeId);
  if (it == m_peers.end())
    return Contact();

  return it->second;
}

}
