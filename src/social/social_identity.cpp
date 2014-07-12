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
#include "social/social_identity.h"

#include <boost/make_shared.hpp>
#include <boost/range/adaptors.hpp>

namespace UniSphere {

SocialIdentity::SocialIdentity(const PrivatePeerKey &key)
  : m_localId(key.nodeId()),
    m_localKey(key)
{
}

bool SocialIdentity::isPeer(const NodeIdentifier &nodeId) const
{
  return m_peers.find(nodeId) != m_peers.end();
}

bool SocialIdentity::isPeer(const Contact &contact) const
{
  return isPeer(contact.nodeId());
}

void SocialIdentity::addPeer(PeerPtr peer)
{
  if (!peer || peer->isNull())
    return;

  RecursiveUniqueLock lock(m_mutex);

  m_peers[peer->nodeId()] = peer;
  signalPeerAdded(peer);
}

void SocialIdentity::addPeer(const Contact &contact)
{
  addPeer(boost::make_shared<Peer>(contact));
}

void SocialIdentity::removePeer(const NodeIdentifier &nodeId)
{
  RecursiveUniqueLock lock(m_mutex);

  m_peers.erase(nodeId);
  signalPeerRemoved(nodeId);
}

Contact SocialIdentity::getPeerContact(const NodeIdentifier &nodeId) const
{
  if (!nodeId.isValid())
    return Contact();

  RecursiveUniqueLock lock(m_mutex);

  auto it = m_peers.find(nodeId);
  if (it == m_peers.end())
    return Contact();

  return it->second->contact();
}

PeerPtr SocialIdentity::getPeer(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);

  auto it = m_peers.find(nodeId);
  if (it == m_peers.end())
    return PeerPtr();

  return it->second;
}

bool SocialIdentity::hasPeerSecurityAssociation(const std::string &publicKey) const
{
  RecursiveUniqueLock lock(m_mutex);
  for (PeerPtr peer : m_peers | boost::adaptors::map_values) {
    if (peer->hasPeerSecurityAssociation(publicKey))
      return true;
  }

  return false;
}

}
