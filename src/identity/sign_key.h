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
#ifndef UNISPHERE_IDENTITY_SIGNKEY_H
#define UNISPHERE_IDENTITY_SIGNKEY_H

#include "core/context.h"
#include "identity/key.h"

namespace UniSphere {

/**
 * Public signing key base.
 */
class UNISPHERE_EXPORT SignKeyBase {
public:
  /// Size of the public key (in bytes)
  static const size_t public_key_length;

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
protected:
  /**
   * Performs public key validation and sets the key to null if it
   * is not a valid key.
   */
  void validatePublic();
protected:
  // Public key storage
  std::string m_public;
};

/// Public signing key
typedef Key<SignKeyBase> SignKey;

/**
 * Private signing key base.
 */
class UNISPHERE_EXPORT PrivateSignKeyBase : public SignKey {
public:
  /// Public key type
  typedef SignKey public_key_type;
  /// Size of the private key (in bytes)
  static const size_t private_key_length;

  // Inherited base constructors
  using SignKey::SignKey;

  /**
   * Generates a new private peer key and overwrites the current key.
   */
  void generate();

  /**
   * Cryptographically signs the specified buffer.
   *
   * @param buffer Buffer to sign
   * @return Cryptographically signed buffer
   * @throws NullPeerKey When attempting to sign with a null key
   */
  std::string sign(const std::string &buffer) const;
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

/// Private signing key
typedef PrivateKey<PrivateSignKeyBase> PrivateSignKey;

}

#endif
