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

// Define the invalid peer instance that can be used for return
// references to invalid peers
const Peer Peer::INVALID = Peer();

Peer::Peer()
{
}

Peer::Peer(const PeerKey &key, const Contact &contact)
  : m_peerKey(key),
    m_contact(contact)
{
  BOOST_ASSERT(key.nodeId() == contact.nodeId());
}

void Peer::setContact(const Contact &contact)
{
  BOOST_ASSERT(contact.nodeId() == m_contact.nodeId());
  m_contact = contact;
}

PeerSecurityAssociationPtr Peer::addPeerSecurityAssociation(const PeerSecurityAssociation &sa)
{
  PeerSecurityAssociationPtr psa = boost::make_shared<PeerSecurityAssociation>(sa);
  m_peerSa.push_front(psa);
  if (m_peerSa.size() > max_security_associations)
    m_peerSa.pop_back();

  return psa;
}

void Peer::removePeerSecurityAssociation(const std::string &publicKey)
{
  if (!m_peerSa.get<1>().erase(publicKey))
    throw InvalidSecurityAssociation("Security association not found!");
}

PeerSecurityAssociationPtr Peer::selectPeerSecurityAssociation(Context &context)
{
  if (m_peerSa.empty())
    return PeerSecurityAssociationPtr();

  std::uniform_int_distribution<int> randomIndex(0, m_peerSa.size() - 1);
  int index = randomIndex(context.basicRng());
  for (auto it = m_peerSa.begin(); it != m_peerSa.end(); ++it) {
    if (index-- == 0)
      return *it;
  }
}

PrivateSecurityAssociationPtr Peer::createPrivateSecurityAssociation(const boost::posix_time::time_duration &expiry)
{
  // Generate a new private key for this association
  PrivateSignKey key;
  key.generate();

  // Create new security association
  PrivateSecurityAssociationPtr sa = boost::make_shared<PrivateSecurityAssociation>(key, expiry);
  m_privateSa.push_front(sa);

  // Remove old security associations
  if (m_privateSa.size() > max_security_associations)
    m_privateSa.pop_back();

  return *m_privateSa.get<1>().find(sa->raw());
}

PrivateSecurityAssociationPtr Peer::getPrivateSecurityAssociation(const std::string &publicKey)
{
  auto it = m_privateSa.get<1>().find(publicKey);
  if (it == m_privateSa.get<1>().end())
    return PrivateSecurityAssociationPtr();

  PrivateSecurityAssociationPtr sa = *it;
  if (sa->isExpired()) {
    m_privateSa.get<1>().erase(it);
    return PrivateSecurityAssociationPtr();
  }

  return sa;
}

}
