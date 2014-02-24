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
#ifndef UNISPHERE_SOCIAL_IDENTITY_H
#define UNISPHERE_SOCIAL_IDENTITY_H

#include "interplex/contact.h"
#include "identity/peer_key.h"
#include "social/peer.h"

#include <boost/signals2/signal.hpp>
#include <unordered_map>

namespace UniSphere {

class UNISPHERE_EXPORT SocialIdentity {
public:
  explicit SocialIdentity(const PrivatePeerKey &key);

  inline const NodeIdentifier &localId() const { return m_localId; }

  inline const PrivatePeerKey &localKey() const { return m_localKey; }

  inline std::unordered_map<NodeIdentifier, Peer> peers() const { return m_peers; }

  bool isPeer(const Contact &contact) const;

  void addPeer(const Peer &peer);

  void addPeer(const PeerKey &key, const Contact &contact);

  void removePeer(const NodeIdentifier &nodeId);

  Contact getPeerContact(const NodeIdentifier &nodeId) const;
public:
  /// Signal that gets called after a new peer is added
  boost::signals2::signal<void(const Peer&)> signalPeerAdded;
  /// Signal that gets called after a peer is removed
  boost::signals2::signal<void(const NodeIdentifier&)> signalPeerRemoved;
private:
  /// Local identifier
  NodeIdentifier m_localId;
  /// Local private peer key
  PrivatePeerKey m_localKey;
  /// Social peers with contact information
  std::unordered_map<NodeIdentifier, Peer> m_peers;
};

}

#endif
