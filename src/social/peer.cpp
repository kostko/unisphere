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
#include "social/peer.h"
#include "social/exceptions.h"

#include <boost/make_shared.hpp>

namespace UniSphere {

Peer::Peer()
{
}

Peer::Peer(const Peer &peer)
  : m_contact(peer.m_contact),
    m_peerSa(peer.m_peerSa),
    m_privateSa(peer.m_privateSa)
{
}

Peer::Peer(const Contact &contact)
  : m_contact(contact)
{
}

Peer &Peer::operator=(Peer other)
{
  std::swap(m_contact, other.m_contact);
  std::swap(m_peerSa, other.m_peerSa);
  std::swap(m_privateSa, other.m_privateSa);
  return *this;
}

void Peer::setContact(const Contact &contact)
{
  BOOST_ASSERT(contact.nodeId() == m_contact.nodeId());

  RecursiveUniqueLock lock(m_mutex);
  m_contact = contact;
}

PeerSecurityAssociationPtr Peer::addPeerSecurityAssociation(const PeerSecurityAssociation &sa)
{
  RecursiveUniqueLock lock(m_mutex);
  PeerSecurityAssociationPtr psa = boost::make_shared<PeerSecurityAssociation>(sa);
  m_peerSa.push_front(psa);
  if (m_peerSa.size() > max_security_associations)
    m_peerSa.pop_back();

  return psa;
}

void Peer::removePeerSecurityAssociation(const std::string &publicKey)
{
  RecursiveUniqueLock lock(m_mutex);
  if (!m_peerSa.get<1>().erase(publicKey))
    throw InvalidSecurityAssociation("Security association not found!");
}

bool Peer::hasPeerSecurityAssociation(const std::string &publicKey) const
{
  RecursiveUniqueLock lock(m_mutex);
  return m_peerSa.get<1>().find(publicKey) != m_peerSa.get<1>().end();
}

PeerSecurityAssociationPtr Peer::selectPeerSecurityAssociation(Context &context)
{
  RecursiveUniqueLock lock(m_mutex);

  if (m_peerSa.empty())
    return PeerSecurityAssociationPtr();

  std::uniform_int_distribution<int> randomIndex(0, m_peerSa.size() - 1);
  int index = randomIndex(context.basicRng());
  for (auto it = m_peerSa.begin(); it != m_peerSa.end(); ++it) {
    if (index-- == 0)
      return *it;
  }
}

PrivateSecurityAssociationPtr Peer::createPrivateSecurityAssociation()
{
  // Generate a new private key for this association
  PrivateSignKey key;
  key.generate();

  PrivateSecurityAssociationPtr sa;
  {
    RecursiveUniqueLock lock(m_mutex);

    // Create new security association
    sa = boost::make_shared<PrivateSecurityAssociation>(key);
    m_privateSa.push_front(sa);

    // Remove old security associations
    if (m_privateSa.size() > max_security_associations)
      m_privateSa.pop_back();
  }

  return sa;
}

PrivateSecurityAssociationPtr Peer::getPrivateSecurityAssociation(const std::string &publicKey)
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_privateSa.get<1>().find(publicKey);
  if (it == m_privateSa.get<1>().end())
    return PrivateSecurityAssociationPtr();

  return *it;
}

std::list<PrivateSecurityAssociationPtr> Peer::getPrivateSecurityAssociations() const
{
  RecursiveUniqueLock lock(m_mutex);
  std::list<PrivateSecurityAssociationPtr> result;
  for (PrivateSecurityAssociationPtr sa : m_privateSa) {
    result.push_back(sa);
  }
  return result;
}

bool Peer::hasPublicSecurityAssociations() const
{
  return !m_peerSa.empty();
}

bool Peer::hasPrivateSecurityAssociations() const
{
  return !m_privateSa.empty();
}

}
