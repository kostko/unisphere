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
#ifndef UNISPHERE_SOCIAL_PEER_H
#define UNISPHERE_SOCIAL_PEER_H

#include "core/context.h"
#include "identity/node_identifier.h"
#include "identity/sign_key.h"
#include "interplex/contact.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace UniSphere {

/**
 * Security association represents a private/public keypair established
 * for a limited duration in scope of a specific link between peers.
 */
template <class KeyType>
class UNISPHERE_EXPORT SecurityAssociation {
public:
  /**
   * Constructs a new security association (SA).
   *
   * @param key Key
   * @param expiry SA expiry
   */
  SecurityAssociation(const KeyType &key, const boost::posix_time::time_duration &expiry)
    : key(key),
      expiryTime(boost::posix_time::microsec_clock::universal_time() + expiry)
  {
  }

  /**
   * Returns the public key as raw bytes.
   */
  inline std::string raw() const
  {
    return key.raw();
  }

  /**
   * Returns true if this security association has already expired.
   */
  inline bool isExpired() const
  {
    return expiryTime <= boost::posix_time::microsec_clock::universal_time();
  }
public:
  /// Time when this security association expires
  const boost::posix_time::ptime expiryTime;
  /// Security association key
  const KeyType key;
};

/// Alias definition for peer security association
typedef SecurityAssociation<SignKey> PeerSecurityAssociation;
typedef boost::shared_ptr<PeerSecurityAssociation> PeerSecurityAssociationPtr;
/// Alias definition for private security association
typedef SecurityAssociation<PrivateSignKey> PrivateSecurityAssociation;
typedef boost::shared_ptr<PrivateSecurityAssociation> PrivateSecurityAssociationPtr;

/**
 * A container for security associations.
 */
template <class SA, class T = typename SA::element_type>
using SecurityAssociations = boost::multi_index_container<
  SA,
  boost::multi_index::indexed_by<
    boost::multi_index::sequenced<>,
    boost::multi_index::hashed_unique<
      boost::multi_index::const_mem_fun<T, std::string, &T::raw>
    >
  >
>;

/// Alias definition for container of peer security associations
typedef SecurityAssociations<PeerSecurityAssociationPtr> PeerSecurityAssociations;
/// Alias definition for container of private security associations
typedef SecurityAssociations<PrivateSecurityAssociationPtr> PrivateSecurityAssociations;

class UNISPHERE_EXPORT Peer {
public:
  /// An invalid peer instance
  static const Peer INVALID;
  /// Maximum number of peer security associations
  static const int max_security_associations = 10;

  /**
   * Constructs a null peer.
   */
  Peer();

  /**
   * Constructs a peer with the specified public key and contact
   * information.
   *
   * @param key Peer public key
   * @param contact Peer contact information
   */
  Peer(const PeerKey &key, const Contact &contact);

  /**
   * Returns true if this is a null peer.
   */
  inline bool isNull() const { return m_peerKey.isNull(); }

  /**
   * Returns the node identifier of this peer.
   */
  inline const NodeIdentifier &nodeId() const { return m_peerKey.nodeId(); }

  /**
   * Returns a reference to this peer's key.
   */
  inline const PeerKey &key() const { return m_peerKey; }

  /**
   * Returns a reference to this peer's contact.
   */
  const Contact &contact() const { return m_contact; }

  /**
   * Updates this peer's contact information.
   *
   * @param contact New contact information
   */
  void setContact(const Contact &contact);

  /**
   * Adds a new peer security association for this peer link.
   *
   * @param sa Peer security association
   * @return A reference to the newly added security association
   */
  PeerSecurityAssociationPtr addPeerSecurityAssociation(const PeerSecurityAssociation &sa);

  /**
   * Removes an existing peer security association identifierd by its
   * public key.
   *
   * @param publicKey Public key identifying the SA
   * @throws InvalidSecurityAssociation When SA cannot be found
   */
  void removePeerSecurityAssociation(const std::string &publicKey);

  /**
   * Randomly selects a valid peer security association and returns it.
   *
   * @param context UNISPHERE context
   * @return A reference to the selected security association
   */
  PeerSecurityAssociationPtr selectPeerSecurityAssociation(Context &context);

  /**
   * Creates a new private security association.
   *
   * @param expiry Expiry time
   * @return A reference to the newly created security association
   */
  PrivateSecurityAssociationPtr createPrivateSecurityAssociation(const boost::posix_time::time_duration &expiry);

  /**
   * Returns a private security association identified by its
   * public key.
   *
   * @param publicKey Public key identifying the private SA
   * @return A pointer to the specified private SA or null when one cannot be found
   */
  PrivateSecurityAssociationPtr getPrivateSecurityAssociation(const std::string &publicKey);
private:
  /// Public key of this peer
  PeerKey m_peerKey;
  /// Contact information for this peer
  Contact m_contact;
  /// Security associations that the peer has chosen for this link
  PeerSecurityAssociations m_peerSa;
  /// Security associations that we have chosen for this link
  PrivateSecurityAssociations m_privateSa;
};

UNISPHERE_SHARED_POINTER(Peer)

}

#endif
