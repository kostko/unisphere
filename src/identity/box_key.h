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
#ifndef UNISPHERE_IDENTITY_BOXKEY_H
#define UNISPHERE_IDENTITY_BOXKEY_H

#include "core/context.h"
#include "identity/exceptions.h"
#include "identity/key.h"

#include <sodium.h>

namespace UniSphere {

/**
 * Boxing key operations.
 */
class UNISPHERE_EXPORT BoxKeyOperations {
protected:
  /**
   * Generates a new public/private boxing key pair.
   *
   * @param publicKey Public key storage
   * @param publicKeyOffset Public key offset within the storage
   * @param privateKey Private key storage
   * @param privateKeyOffset Private key offset within the storage
   */
  void opBoxGenerate(std::string &publicKey,
                     size_t publicKeyOffset,
                     std::string &privateKey,
                     size_t privateKeyOffset) const;

  /**
   * Creates a cryptographic box containing the specified buffer.
   *
   * @param publicKey Public key of the recipient
   * @param publicKeyOffset Public key offset within the storage
   * @param privateKey Private key storage
   * @param privateKeyOffset Private key offset within the storage
   * @param buffer Buffer to store into the box
   * @return Cryptographic box
   */
  std::string opBoxEncrypt(const std::string &publicKey,
                           size_t publicKeyOffset,
                           const std::string &privateKey,
                           size_t privateKeyOffset,
                           const std::string &buffer) const;

  /**
   * Opens a cryptographic box.
   *
   * @param publicKey Public key of the sender
   * @param publicKeyOffset Public key offset within the storage
   * @param privateKey Private key storage
   * @param privateKeyOffset Private key offset within the storage
   * @param buffer Buffer containing the cryptographic box
   * @return Decrypted box contents
   */
  std::string opBoxOpen(const std::string &publicKey,
                        size_t publicKeyOffset,
                        const std::string &privateKey,
                        size_t privateKeyOffset,
                        const std::string &buffer) const;
};

/**
 * Public boxing key.
 */
class UNISPHERE_EXPORT PublicBoxKey : public virtual PublicKey<crypto_box_PUBLICKEYBYTES>,
                                      public BoxKeyOperations
{
public:
  using PublicKey::PublicKey;
};

/**
 * Private boxing key.
 */
class UNISPHERE_EXPORT PrivateBoxKey : public PublicBoxKey,
                                       public PrivateKey<PublicBoxKey, crypto_box_SECRETKEYBYTES>
{
public:
  using PrivateKey::PrivateKey;

  /**
   * Generates a new private box key and overwrites the current key.
   */
  void generate();

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
  std::string boxOpen(const PublicBoxKey &otherPublicKey,
                      const std::string &buffer) const;
};

}

#endif
