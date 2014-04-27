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

const size_t PeerKeyBase::public_key_length = crypto_box_PUBLICKEYBYTES;
const size_t PrivatePeerKeyBase::private_key_length = crypto_box_SECRETKEYBYTES;

const NodeIdentifier &PeerKeyBase::nodeId() const
{
  if (m_public.empty())
    return NodeIdentifier::INVALID;

  // Generate a node identifier and cache it
  if (m_nodeId.isNull()) {
    Botan::Pipe pipe(new Botan::Hash_Filter("SHA-512"));
    pipe.process_msg(m_public);
    m_nodeId = NodeIdentifier(pipe.read_all_as_string(0).substr(0, NodeIdentifier::length));
  }

  return m_nodeId;
}

std::string PeerKeyBase::encrypt(const std::string &buffer) const
{
  if (m_public.empty())
    throw NullKey("Attempted to encrypt with a null key!");

  // TODO

  return std::string();
}

void PeerKeyBase::validatePublic()
{
  if (m_public.size() != PeerKeyBase::public_key_length)
    m_public.clear();
}

void PrivatePeerKeyBase::generate()
{
  unsigned char pubkey[PeerKey::public_key_length];
  m_private.resize(PrivatePeerKeyBase::private_key_length);
  crypto_box_keypair(pubkey, m_private);
  m_public = std::string((char*) pubkey, sizeof(pubkey));
}

std::string PrivatePeerKeyBase::boxOpen(const std::string &encryptedBuffer) const
{
  if (m_private.empty())
    throw NullKey("Attempted to open box with a null key!");

  // TODO

  return std::string();
}

void PrivatePeerKeyBase::validatePrivate()
{
  if (m_public.empty() || m_private.size() != PrivatePeerKeyBase::private_key_length) {
    m_public.clear();
    m_private.clear();
  }
}

}
