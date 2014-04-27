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
#include "identity/exceptions.h"

#include <botan/botan.h>
#include <sodium.h>

namespace UniSphere {

const size_t SignKeyBase::public_key_length = crypto_sign_PUBLICKEYBYTES;
const size_t PrivateSignKeyBase::private_key_length = crypto_sign_SECRETKEYBYTES;

std::string SignKeyBase::signOpen(const std::string &signedBuffer) const
{
  if (m_public.empty())
    throw NullKey("Attempted to open with a null key!");

  size_t smlen = signedBuffer.size();
  unsigned char m[smlen];
  unsigned long long mlen;

  if (crypto_sign_open(m, &mlen, (unsigned char*) signedBuffer.c_str(), smlen, (unsigned char*) m_public.c_str()) != 0)
    throw InvalidSignature("Invalid signature!");

  return std::string((char*) m, mlen);
}

void SignKeyBase::validatePublic()
{
  if (m_public.size() != SignKeyBase::public_key_length)
    m_public.clear();
}

std::string PrivateSignKeyBase::sign(const std::string &buffer) const
{
  if (m_private.empty())
    throw NullKey("Attempted to sign with a null key!");

  unsigned char sm[buffer.size() + crypto_sign_BYTES];
  unsigned long long smlen;
  crypto_sign(sm, &smlen, (unsigned char*) buffer.c_str(), buffer.size(), m_private);

  return std::string((char*) sm, smlen);
}

void PrivateSignKeyBase::generate()
{
  unsigned char pubkey[SignKey::public_key_length];
  m_private.resize(PrivateSignKeyBase::private_key_length);
  crypto_sign_keypair(pubkey, m_private);
  m_public = std::string((char*) pubkey, sizeof(pubkey));
}

void PrivateSignKeyBase::validatePrivate()
{
  if (m_public.empty() || m_private.size() != PrivateSignKeyBase::private_key_length) {
    m_public.clear();
    m_private.clear();
  }
}

}
