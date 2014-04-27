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
#ifndef UNISPHERE_IDENTITY_PEERKEY_H
#define UNISPHERE_IDENTITY_PEERKEY_H

#include "core/context.h"
#include "identity/node_identifier.h"
#include "identity/key.h"

namespace UniSphere {

/**
 * Public peer key base.
 */
class UNISPHERE_EXPORT PeerKeyBase {
public:
  /// Size of the public key (in bytes)
  static const size_t public_key_length;

  /**
   * Returns a node identifier that identifies this peer key.
   */
  const NodeIdentifier &nodeId() const;

  /**
   * Encrypts the specified buffer.
   *
   * @param buffer Buffer to encrypt
   * @return Encrypted buffer
   * @throws NullPeerKey When attempting to encrypt with a null key
   */
  std::string encrypt(const std::string &buffer) const;
protected:
  /**
   * Performs public key validation and sets the key to null if it
   * is not a valid key.
   */
  void validatePublic();
protected:
  // Public key storage
  std::string m_public;
  // Cached node identifier
  mutable NodeIdentifier m_nodeId;
};

/// Public peer key
typedef Key<PeerKeyBase> PeerKey;

/**
 * Private peer key base.
 */
class UNISPHERE_EXPORT PrivatePeerKeyBase : public PeerKey {
public:
  /// Public key type
  typedef PeerKey public_key_type;
  /// Size of the private key (in bytes)
  static const size_t private_key_length;

  // Inherited base constructors
  using PeerKey::PeerKey;

  /**
   * Generates a new private peer key and overwrites the current key.
   */
  void generate();

  /**
   * Opens an encrypted box.
   *
   * @param encryptedBuffer Encrypted box
   * @return Plaintext
   * @throws InvalidSignature When the signature is not valid
   * @throws NullPeerKey When attempting to open a box with a null key
   */
  std::string boxOpen(const std::string &encryptedBuffer) const;
protected:
  /**
   * Performs private key validation and sets the key to null if it
   * is not a valid key.
   */
  void validatePrivate();
protected:
  /// Private key storage
  KeyData m_private;
};

/// Private peer key
typedef PrivateKey<PrivatePeerKeyBase> PrivatePeerKey;

}

#endif
