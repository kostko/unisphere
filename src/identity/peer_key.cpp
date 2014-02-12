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
#include "identity/peer_key.h"
#include "identity/exceptions.h"

#include <botan/botan.h>
#include <sodium.h>

namespace UniSphere {

// Define the invalid peer key instance that can be used for return
// references to invalid peer keys
const PeerKey PeerKey::INVALID = PeerKey();

const size_t PeerKey::sign_public_key_length = crypto_sign_ed25519_PUBLICKEYBYTES;
const size_t PrivatePeerKey::sign_private_key_length = crypto_sign_ed25519_SECRETKEYBYTES;

PeerKey::PeerKey()
{
}

PeerKey::PeerKey(const std::string &publicSignKey)
  : PeerKey(publicSignKey, Format::Raw)
{
}

PeerKey::PeerKey(const std::string &publicSignKey, Format format)
{
  switch (format) {
    // Raw bytes format
    case Format::Raw: m_publicSign = publicSignKey; break;

    // Base64 format
    case Format::Base64: {
      try {
        Botan::Pipe pipe(new Botan::Base64_Decoder());
        pipe.process_msg(publicSignKey);
        m_publicSign = pipe.read_all_as_string(0);
      } catch (std::exception &e) {
        m_publicSign.clear();
      }
      break;
    }
  }

  if (m_publicSign.size() != PeerKey::sign_public_key_length)
    m_publicSign.clear();
}

const NodeIdentifier &PeerKey::nodeId()
{
  if (isNull())
    return NodeIdentifier::INVALID;

  // Generate a node identifier and cache it
  Botan::Pipe pipe(new Botan::Hash_Filter("SHA-512"));
  pipe.process_msg(m_publicSign);
  m_nodeId = NodeIdentifier(pipe.read_all_as_string(0).substr(0, NodeIdentifier::length));

  return m_nodeId;
}

std::string PeerKey::signBase32() const
{
  // TODO
  return std::string();
}

std::string PeerKey::signBase64() const
{
  Botan::Pipe pipe(new Botan::Base64_Encoder());
  pipe.process_msg(m_publicSign);
  return pipe.read_all_as_string(0);
}

std::string PeerKey::signOpen(const std::string &signedBuffer) const
{
  if (isNull())
    throw NullPeerKey("Attempted to open with a null key!");

  size_t smlen = signedBuffer.size();
  unsigned char m[smlen];
  unsigned long long mlen;

  if (crypto_sign_ed25519_open(m, &mlen, (unsigned char*) signedBuffer.c_str(), smlen, (unsigned char*) m_publicSign.c_str()) != 0)
    throw InvalidSignature("Invalid signature!");

  return std::string((char*) m, mlen);
}

std::string PeerKey::encrypt(const std::string &buffer) const
{
  if (isNull())
    throw NullPeerKey("Attempted to encrypt with a null key!");

  // TODO

  return std::string();
}

PrivatePeerKey::PrivatePeerKey()
  : PeerKey(),
    m_privateSign(PrivatePeerKey::sign_private_key_length)
{
}

PrivatePeerKey::PrivatePeerKey(const std::string &publicSignKey,
                               const Botan::SecureVector<unsigned char> &privateSignKey)
  : PrivatePeerKey(publicSignKey, privateSignKey, Format::Raw)
{
}

PrivatePeerKey::PrivatePeerKey(const std::string &publicSignKey,
                               const Botan::SecureVector<unsigned char> &privateSignKey,
                               Format format)
  : PeerKey(publicSignKey, format)
{
  switch (format) {
    // Raw bytes format
    case Format::Raw: m_privateSign = privateSignKey; break;

    // Base64 format
    case Format::Base64: {
      try {
        Botan::Pipe pipe(new Botan::Base64_Decoder());
        pipe.process_msg(privateSignKey);
        m_privateSign = pipe.read_all(0);
      } catch (std::exception &e) {
        m_privateSign.clear();
      }
      break;
    }
  }

  if (m_publicSign.empty() || m_privateSign.size() != PrivatePeerKey::sign_private_key_length) {
    m_publicSign.clear();
    m_privateSign.clear();
  }
}

KeyData PrivatePeerKey::signPrivateBase32() const
{
  // TODO
  return KeyData();
}

KeyData PrivatePeerKey::signPrivateBase64() const
{
  Botan::Pipe pipe(new Botan::Base64_Encoder());
  pipe.process_msg(m_privateSign);
  return pipe.read_all(0);
}

void PrivatePeerKey::generate()
{
  unsigned char pubkey[PeerKey::sign_public_key_length];
  crypto_sign_ed25519_keypair(pubkey, m_privateSign);
  m_publicSign = std::string((char*) pubkey, sizeof(pubkey));
}

void PrivatePeerKey::importFile(const std::string &filename,
                                const Botan::SecureVector<unsigned char> &password)
{
  // TODO
}

std::string PrivatePeerKey::sign(const std::string &buffer) const
{
  if (isNull())
    throw NullPeerKey("Attempted to sign with a null key!");

  unsigned char sm[buffer.size() + crypto_sign_ed25519_BYTES];
  unsigned long long smlen;
  crypto_sign_ed25519(sm, &smlen, (unsigned char*) buffer.c_str(), buffer.size(), m_privateSign);

  return std::string((char*) sm, smlen);
}

std::string PrivatePeerKey::boxOpen(const std::string &encryptedBuffer) const
{
  if (isNull())
    throw NullPeerKey("Attempted to open box with a null key!");

  // TODO

  return std::string();
}

void PrivatePeerKey::exportFile(const std::string &filename,
                                const Botan::SecureVector<unsigned char> &password) const
{
  if (isNull())
    throw NullPeerKey("Attempted to export a null key!");

  // TODO
}

}
