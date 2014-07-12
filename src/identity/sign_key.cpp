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
#include "identity/sign_key.h"

namespace UniSphere {

void SignKeyOperations::opSignGenerate(std::string &publicKey,
                                       size_t publicKeyOffset,
                                       std::string &privateKey,
                                       size_t privateKeyOffset) const
{
  crypto_sign_keypair(
    (unsigned char*) &publicKey[publicKeyOffset],
    (unsigned char*) &privateKey[privateKeyOffset]
  );
}

std::string SignKeyOperations::opSign(const std::string &privateKey,
                                      size_t privateKeyOffset,
                                      const std::string &buffer) const
{
#ifdef UNISPHERE_CRYPTO_NOOP
  return buffer;
#else
  unsigned char sm[buffer.size() + crypto_sign_BYTES];
  unsigned long long smlen;
  crypto_sign(sm, &smlen, (unsigned char*) buffer.data(), buffer.size(), (unsigned char*) &privateKey[privateKeyOffset]);

  return std::string((char*) sm, smlen);
#endif
}

std::string SignKeyOperations::opSignOpen(const std::string &publicKey,
                                          size_t publicKeyOffset,
                                          const std::string &buffer) const
{
#ifdef UNISPHERE_CRYPTO_NOOP
  return buffer;
#else
  size_t smlen = buffer.size();
  unsigned char m[smlen];
  unsigned long long mlen;

  if (crypto_sign_open(m, &mlen, (unsigned char*) buffer.data(), smlen, (unsigned char*) &publicKey[publicKeyOffset]) != 0)
    throw InvalidSignature("Invalid signature!");

  return std::string((char*) m, mlen);
#endif
}

std::string PublicSignKey::signOpen(const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opSignOpen(m_public, 0, buffer);
}

void PrivateSignKey::generate()
{
  if (isNull()) {
    m_public.resize(Public::KeySize);
    m_private.resize(KeySize);
  }

  opSignGenerate(m_public, 0, m_private, 0);
}

std::string PrivateSignKey::sign(const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opSign(m_private, 0, buffer);
}

std::string PrivateSignKey::sign(const google::protobuf::Message &msg) const
{
  std::string buffer;
  msg.SerializeToString(&buffer);

  return sign(buffer);
}

}
