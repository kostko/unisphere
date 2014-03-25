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
  /**
   * Constructs a new social identity.
   *
   * @param key Private peer key
   */
  explicit SocialIdentity(const PrivatePeerKey &key);

  /**
   * Returns the node identifier of the local node.
   */
  inline const NodeIdentifier &localId() const { return m_localId; }

  /**
   * Returns the private key of the local node.
   */
  inline const PrivatePeerKey &localKey() const { return m_localKey; }

  /**
   * Returns a specific peer instance.
   *
   * @param nodeId Peer's node identifier
   * @return Peer instance
   */
  PeerPtr getPeer(const NodeIdentifier &nodeId) const;

  /**
   * Returns a map of node identifiers to peer instances.
   */
  inline std::unordered_map<NodeIdentifier, PeerPtr> peers() const { return m_peers; }

  bool isPeer(const Contact &contact) const;

  void addPeer(PeerPtr peer);

  void addPeer(const PeerKey &key, const Contact &contact);

  void removePeer(const NodeIdentifier &nodeId);

  Contact getPeerContact(const NodeIdentifier &nodeId) const;
public:
  /// Signal that gets called after a new peer is added
  boost::signals2::signal<void(PeerPtr)> signalPeerAdded;
  /// Signal that gets called after a peer is removed
  boost::signals2::signal<void(const NodeIdentifier&)> signalPeerRemoved;
private:
  /// Mutex protecting the social identity
  mutable std::recursive_mutex m_mutex;
  /// Local identifier
  NodeIdentifier m_localId;
  /// Local private peer key
  PrivatePeerKey m_localKey;
  /// Social peers with contact information
  std::unordered_map<NodeIdentifier, PeerPtr> m_peers;
};

}

#endif
