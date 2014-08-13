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
#include <unordered_set>

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

  /**
   * Returns true if a given contact is a known peer.
   *
   * @param nodeId Peer node identifier
   * @return True if node is a valid peer and false otherwise
   */
  bool isPeer(const NodeIdentifier &nodeId) const;

  /**
   * Returns true if a given contact is a known peer.
   *
   * @param contact Contact data
   * @return True if contact is a valid peer and false otherwise
   */
  bool isPeer(const Contact &contact) const;

  /**
   * Adds a new peer.
   *
   * @param peer Peer instance to add
   */
  void addPeer(PeerPtr peer);

  /**
   * Adds a new peer.
   *
   * @param contact Peer contact data
   */
  void addPeer(const Contact &contact);

  /**
   * Removes an existing peer.
   *
   * @param nodeId Peer's node identifier
   */
  void removePeer(const NodeIdentifier &nodeId);

  /**
   * Returns the peer contact for a given peer.
   *
   * @param nodeId Peer's node identifier
   */
  Contact getPeerContact(const NodeIdentifier &nodeId) const;

  /**
   * Adds a new peer security association for the specified peer.
   *
   * @param peer Peer instance
   * @param sa Peer security association
   * @return A reference to the newly added security association
   */
  PeerSecurityAssociationPtr addPeerSecurityAssociation(PeerPtr peer, const PeerSecurityAssociation &sa);

  /**
   * Removes an existing peer security association identifierd by its
   * public key.
   *
   * @param peer Peer instance
   * @param publicKey Public key identifying the SA
   * @throws InvalidSecurityAssociation When SA cannot be found
   */
  void removePeerSecurityAssociation(PeerPtr peer, const std::string &publicKey);

  /**
   * Returns true if there exist a peer security association with the
   * given public key.
   *
   * @param publicKey SA public key
   * @return True if such SA exists, false otherwise
   */
  bool hasPeerSecurityAssociation(const std::string &publicKey) const;
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
  /// Peer security association cache
  std::unordered_set<std::string> m_peerSaCache;
};

}

#endif
