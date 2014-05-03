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
#include "identity/sign_key.h"
#include "identity/box_key.h"

namespace UniSphere {

/**
 * Public peer key.
 */
class UNISPHERE_EXPORT PublicPeerKey : public virtual PublicKey<PublicSignKey::KeySize + PublicBoxKey::KeySize>,
                                       public SignKeyOperations,
                                       public BoxKeyOperations
{
public:
  using PublicKey::PublicKey;

  /**
   * Returns a copy of the public signing subkey.
   */
  PublicSignKey signSubkey() const;

  /**
   * Returns a copy of the public boxing subkey.
   */
  PublicBoxKey boxSubkey() const;

  /**
   * Returns a node identifier that identifies this peer key.
   */
  const NodeIdentifier &nodeId() const;

  /**
   * Verifies the cryptographically signed buffer and returns the payload
   * in case verification succeeds.
   *
   * @param buffer Cryptographically signed buffer
   * @return Payload
   * @throws InvalidSignature When the signature is not valid
   */
  std::string signOpen(const std::string &buffer) const;
protected:
  /// Cached node identifier
  mutable NodeIdentifier m_nodeId;
};

/**
 * Private peer key.
 */
class UNISPHERE_EXPORT PrivatePeerKey : public PublicPeerKey,
                                        public PrivateKey<PublicPeerKey, PrivateSignKey::KeySize + PrivateBoxKey::KeySize>
{
public:
  using PrivateKey::PrivateKey;

  /**
   * Generates a new private peer key and overwrites the current key.
   */
  void generate();

  /**
   * Returns a copy of the private signing subkey.
   */
  PrivateSignKey privateSignSubkey() const;

  /**
   * Returns a copy of the private boxing subkey.
   */
  PrivateBoxKey privateBoxSubkey() const;

  /**
   * Cryptographically signs the specified buffer.
   *
   * @param buffer Buffer to sign
   * @return Cryptographically signed buffer
   */
  std::string sign(const std::string &buffer) const;

  /**
   * Creates a cryptographic box containing the specified buffer.
   *
   * @param otherPublicKey Public key of the recipient
   * @param buffer Buffer to store into the box
   * @return Cryptographic box
   */
  std::string boxEncrypt(const PublicPeerKey &otherPublicKey,
                         const std::string &buffer) const;

  /**
   * Creates a cryptographic box containing the specified buffer.
   *
   * @param otherPublicKey Public key of the recipient
   * @param buffer Buffer to store into the box
   * @return Cryptographic box
   */
  std::string boxEncrypt(const PublicBoxKey &otherPublicKey,
                         const std::string &buffer) const;

  /**
   * Opens a cryptographic box.
   *
   * @param otherPublicKey Public key of the sender
   * @param buffer Buffer containing the cryptographic box
   * @return Decrypted box contents
   */
  std::string boxOpen(const PublicPeerKey &otherPublicKey,
                      const std::string &buffer) const;

  /**
   * Opens a cryptographic box.
   *
   * @param otherPublicKey Public key of the sender
   * @param buffer Buffer containing the cryptographic box
   * @return Decrypted box contents
   */
  std::string boxOpen(const PublicBoxKey &otherPublicKey,
                      const std::string &buffer) const;
};

}

#endif
