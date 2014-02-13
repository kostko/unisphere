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

#include <botan/secmem.h>

namespace UniSphere {

/// A secure vector for storing key data
typedef Botan::SecureVector<unsigned char> KeyData;

/**
 * Public peer key.
 */
class UNISPHERE_EXPORT PeerKey {
public:
  /// An invalid peer key instance
  static const PeerKey INVALID;
  /// Size of the public signing key (in bytes)
  static const size_t sign_public_key_length;

  /**
   * Format specifications for dealing with peer keys.
   */
  enum class Format {
    Raw,
    Base64
  };

  /**
   * Constructs a null peer key.
   */
  PeerKey();

  /**
   * Constructs a new peer key instance from a raw public key buffer.
   *
   * @param publicSignKey Raw public signing key buffer
   */
  explicit PeerKey(const std::string &publicSignKey);

  /**
   * Constructs a new peer key instance.
   *
   * @param publicSignKey Public signing key in the specified format
   * @param format Key format
   */
  PeerKey(const std::string &publicSignKey, Format format);

  /**
   * Returns true if the key is a null key.
   */
  inline bool isNull() const { return m_publicSign.empty(); }

  /**
   * Returns the public signing key as raw bytes.
   */
  inline std::string signRaw() const { return m_publicSign; };

  /**
   * Returns the public signing key as a base32 encoded string.
   */
  std::string signBase32() const;

  /**
   * Returns the public signing key as a base64 encoded string.
   */
  std::string signBase64() const;

  /**
   * Returns a node identifier that identifies this peer key.
   */
  const NodeIdentifier &nodeId();

  /**
   * Verifies the cryptographically signed buffer and returns the payload
   * in case verification succeeds.
   *
   * @param signedBuffer Cryptographically signed buffer
   * @return Payload
   * @throws InvalidSignature When the signature is not valid
   * @throws NullPeerKey When attempting to verify with a null key
   */
  std::string signOpen(const std::string &signedBuffer) const;

  /**
   * Encrypts the specified buffer.
   *
   * @param buffer Buffer to encrypt
   * @return Encrypted buffer
   * @throws NullPeerKey When attempting to encrypt with a null key
   */
  std::string encrypt(const std::string &buffer) const;

  /**
   * Returns true if public peer keys are equal.
   */
  bool operator==(const PeerKey &other) const;
protected:
  // Public signing key storage
  std::string m_publicSign;
  // Cached node identifier
  NodeIdentifier m_nodeId;
};

/**
 * Private peer key.
 */
class UNISPHERE_EXPORT PrivatePeerKey : public PeerKey {
public:
  /// Size of the private signing key (in bytes)
  static const size_t sign_private_key_length;

  /**
   * Constructs a null private peer key.
   */
  PrivatePeerKey();

  /**
   * Constructs a new peer key instance from a raw public and private
   * key buffers.
   *
   * @param publicSignKey Raw public signing key buffer
   * @param privateSignKey Raw private signing key buffer
   */
  PrivatePeerKey(const std::string &publicSignKey,
                 const KeyData &privateSignKey);

  /**
   * Constructs a new peer key instance.
   *
   * @param publicSignKey Public signing key in the specified format
   * @param privateSignKey Private signing key in the specified format
   * @param format Key format
   */
  PrivatePeerKey(const std::string &publicSignKey,
                 const KeyData &privateSignKey,
                 Format format);

  /**
   * Returns the private signing key as raw bytes.
   */
  inline KeyData signPrivateRaw() const { return m_privateSign; };

  /**
   * Returns the private signing key as a base32 encoded string.
   */
  KeyData signPrivateBase32() const;

  /**
   * Returns the private signing key as a base64 encoded string.
   */
  KeyData signPrivateBase64() const;

  /**
   * Generates a new private peer key and overwrites the current key.
   */
  void generate();

  /**
   * Returns a peer key with only the public key part.
   *
   * @throws NullPeerKey When attempting to call with a null key
   */
  inline PeerKey publicKey() const { return PeerKey(m_publicSign); }

  /**
   * Cryptographically signs the specified buffer.
   *
   * @param buffer Buffer to sign
   * @return Cryptographically signed buffer
   * @throws NullPeerKey When attempting to sign with a null key
   */
  std::string sign(const std::string &buffer) const;

  /**
   * Opens an encrypted box.
   *
   * @param encryptedBuffer Encrypted box
   * @return Plaintext
   * @throws InvalidSignature When the signature is not valid
   * @throws NullPeerKey When attempting to open a box with a null key
   */
  std::string boxOpen(const std::string &encryptedBuffer) const;

  /**
   * Returns true if private peer keys are equal.
   */
  bool operator==(const PrivatePeerKey &other) const;
private:
  /// Private signing key storage
  KeyData m_privateSign;
};

/**
 * Operator for private key serialization.
 */
UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &stream, const PrivatePeerKey &key);

/**
 * Operator for private key deserialization.
 */
UNISPHERE_EXPORT std::istream &operator>>(std::istream &stream, PrivatePeerKey &key);

}

#endif
