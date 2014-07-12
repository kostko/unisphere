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
#include "identity/exceptions.h"
#include "identity/key.h"

#include <sodium.h>
#include <google/protobuf/message.h>

namespace UniSphere {

/**
 * Signing key operations.
 */
class UNISPHERE_EXPORT SignKeyOperations {
protected:
  /**
   * Generates a new public/private signing key pair.
   *
   * @param publicKey Public key storage
   * @param publicKeyOffset Public key offset within the storage
   * @param privateKey Private key storage
   * @param privateKeyOffset Private key offset within the storage
   */
  void opSignGenerate(std::string &publicKey,
                      size_t publicKeyOffset,
                      std::string &privateKey,
                      size_t privateKeyOffset) const;

  /**
   * Cryptographically signs the specified buffer.
   *
   * @param privateKey Private key storage
   * @param privateKeyOffset Private key offset within the storage
   * @param buffer Buffer to sign
   * @return Cryptographically signed buffer
   */
  std::string opSign(const std::string &privateKey,
                     size_t privateKeyOffset,
                     const std::string &buffer) const;

  /**
   * Verifies the cryptographically signed buffer and returns the payload
   * in case verification succeeds.
   *
   * @param publicKey Public key storage
   * @param publicKeyOffset Public key offset within the storage
   * @param buffer Cryptographically signed buffer
   * @return Payload
   * @throws InvalidSignature When the signature is not valid
   */
  std::string opSignOpen(const std::string &publicKey,
                         size_t publicKeyOffset,
                         const std::string &buffer) const;
};

/**
 * Public signing key.
 */
class UNISPHERE_EXPORT PublicSignKey : public virtual PublicKey<crypto_sign_PUBLICKEYBYTES>,
                                       public SignKeyOperations
{
public:
  using PublicKey::PublicKey;

  /**
   * Verifies the cryptographically signed buffer and returns the payload
   * in case verification succeeds.
   *
   * @param buffer Cryptographically signed buffer
   * @return Payload
   * @throws InvalidSignature When the signature is not valid
   */
  std::string signOpen(const std::string &buffer) const;
};

/**
 * Private signing key.
 */
class UNISPHERE_EXPORT PrivateSignKey : public PublicSignKey,
                                        public PrivateKey<PublicSignKey, crypto_sign_SECRETKEYBYTES>
{
public:
  using PrivateKey::PrivateKey;

  /**
   * Generates a new private sign key and overwrites the current key.
   */
  void generate();

  /**
   * Cryptographically signs the specified buffer. The resulting buffer
   * includes the contents.
   *
   * @param buffer Buffer to sign
   * @return Cryptographically signed buffer
   */
  std::string sign(const std::string &buffer) const;

  /**
   * Cryptographically signs the specified Protocol Buffers message. The
   * resulting buffer includes the contents.
   *
   * @param msg Protocol Buffers message to sign
   * @return Cryptographically signed buffer
   */
  std::string sign(const google::protobuf::Message &msg) const;
};

}

#endif
