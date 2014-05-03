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
#include "identity/box_key.h"

namespace UniSphere {

void BoxKeyOperations::opBoxGenerate(std::string &publicKey,
                                     size_t publicKeyOffset,
                                     std::string &privateKey,
                                     size_t privateKeyOffset) const
{
  crypto_box_keypair(
    (unsigned char*) &publicKey[publicKeyOffset],
    (unsigned char*) &privateKey[privateKeyOffset]
  );
}

std::string BoxKeyOperations::opBoxEncrypt(const std::string &publicKey,
                                           size_t publicKeyOffset,
                                           const std::string &privateKey,
                                           size_t privateKeyOffset,
                                           const std::string &buffer) const
{
  // TODO
  return std::string();
}

std::string BoxKeyOperations::opBoxOpen(const std::string &publicKey,
                                        size_t publicKeyOffset,
                                        const std::string &privateKey,
                                        size_t privateKeyOffset,
                                        const std::string &buffer) const
{
  // TODO
  return std::string();
}

void PrivateBoxKey::generate()
{
  if (isNull()) {
    m_public.resize(Public::KeySize);
    m_private.resize(KeySize);
  }

  opBoxGenerate(m_public, 0, m_private, 0);
}

std::string PrivateBoxKey::boxEncrypt(const PublicBoxKey &otherPublicKey,
                                      const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxEncrypt(otherPublicKey.raw(), 0, m_private, 0, buffer);
}

std::string PrivateBoxKey::boxOpen(const PublicBoxKey &otherPublicKey,
                                   const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxOpen(otherPublicKey.raw(), 0, m_private, 0, buffer);
}

}
