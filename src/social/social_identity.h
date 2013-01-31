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
#ifndef UNISPHERE_SOCIAL_IDENTITY_H
#define UNISPHERE_SOCIAL_IDENTITY_H

#include "interplex/contact.h"

#include <boost/signal.hpp>
#include <unordered_map>

namespace UniSphere {

class UNISPHERE_EXPORT SocialIdentity {
public:
  /**
   * Copy constructor.
   */
  SocialIdentity(const SocialIdentity &identity);

  explicit SocialIdentity(const NodeIdentifier &localId);
  
  inline NodeIdentifier localId() const { return m_localId; }
  
  inline std::unordered_map<NodeIdentifier, Contact> peers() const { return m_peers; }

  bool isPeer(const Contact &contact) const;
  
  void addPeer(const Contact &peer);
  
  void removePeer(const NodeIdentifier &nodeId);

  Contact getPeerContact(const NodeIdentifier &nodeId) const;

  // TODO: Trust weights should be added to individual peers

  // TODO: There should be a way to persist the social identity
public:
  /// Signal that gets called after a new peer is added
  boost::signal<void(const Contact&)> signalPeerAdded;
  /// Signal that gets called after a peer is removed
  boost::signal<void(const NodeIdentifier&)> signalPeerRemoved;
private:
  /// Local identifier
  NodeIdentifier m_localId;
  /// Social peers with contact information
  std::unordered_map<NodeIdentifier, Contact> m_peers;
};
  
}

#endif
