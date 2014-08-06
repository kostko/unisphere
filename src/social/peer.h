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
   */
  explicit SecurityAssociation(const KeyType &key)
    : key(key)
  {
  }

  /**
   * Returns the public key as raw bytes.
   */
  inline std::string raw() const
  {
    return key.raw();
  }
public:
  /// Security association key
  const KeyType key;
};

/// Alias definition for peer security association
using PeerSecurityAssociation = SecurityAssociation<PublicSignKey>;
using PeerSecurityAssociationPtr = boost::shared_ptr<PeerSecurityAssociation>;
/// Alias definition for private security association
using PrivateSecurityAssociation = SecurityAssociation<PrivateSignKey>;
using PrivateSecurityAssociationPtr = boost::shared_ptr<PrivateSecurityAssociation>;

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
using PeerSecurityAssociations = SecurityAssociations<PeerSecurityAssociationPtr>;
/// Alias definition for container of private security associations
using PrivateSecurityAssociations = SecurityAssociations<PrivateSecurityAssociationPtr>;

class UNISPHERE_EXPORT Peer {
public:
  /// Maximum number of peer security associations
  static const int max_peer_security_associations = 10;
  /// Maximum number of private security associations
  static const int max_private_security_associations = 13;

  /**
   * Constructs a null peer.
   */
  Peer();

  /**
   * Copy constructor.
   */
  Peer(const Peer &peer);

  /**
   * Move constructor.
   */
  Peer(Peer &&peer) = default;

  /**
   * Constructs a peer with the specified public key and contact
   * information.
   *
   * @param contact Peer contact information
   */
  explicit Peer(const Contact &contact);

  /**
   * Copy/move assignment operator.
   */
  Peer &operator=(Peer other);

  /**
   * Returns true if this is a null peer.
   */
  bool isNull() const { return m_contact.isNull(); }

  /**
   * Returns the node identifier of this peer.
   */
  NodeIdentifier nodeId() const { return m_contact.nodeId(); }

  /**
   * Returns this peer's key.
   */
  PublicPeerKey key() const { return m_contact.peerKey(); }

  /**
   * Returns this peer's contact.
   */
  Contact contact() const { return m_contact; }

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
   * Returns true if there exists a security association with the given
   * public key.
   *
   * @param publicKey SA public key
   * @return True if such SA exists, false otherwise
   */
  bool hasPeerSecurityAssociation(const std::string &publicKey) const;

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
   * @return A reference to the newly created security association
   */
  PrivateSecurityAssociationPtr createPrivateSecurityAssociation();

  /**
   * Returns a private security association identified by its
   * public key.
   *
   * @param publicKey Public key identifying the private SA
   * @return A pointer to the specified private SA or null when one cannot be found
   */
  PrivateSecurityAssociationPtr getPrivateSecurityAssociation(const std::string &publicKey);

  /**
   * Returns a list of private security associations.
   */
  std::list<PrivateSecurityAssociationPtr> getPrivateSecurityAssociations() const;

  /**
   * Returns true if we have stored any peer-SAs for this link.
   */
  bool hasPublicSecurityAssociations() const;

  /**
   * Returns true if we have generated any SAs for this link.
   */
  bool hasPrivateSecurityAssociations() const;
private:
  /// Contact information for this peer
  Contact m_contact;
  /// Security associations that the peer has chosen for this link
  PeerSecurityAssociations m_peerSa;
  /// Security associations that we have chosen for this link
  PrivateSecurityAssociations m_privateSa;
  /// Mutex protecting this peer
  mutable std::recursive_mutex m_mutex;
};

UNISPHERE_SHARED_POINTER(Peer)

}

#endif
