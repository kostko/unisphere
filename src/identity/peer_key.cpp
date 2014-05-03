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

namespace UniSphere {

PublicSignKey PublicPeerKey::signSubkey() const
{
  if (isNull())
    return PublicSignKey();

  return PublicSignKey(std::string(m_public, 0, PublicSignKey::KeySize));
}

PublicBoxKey PublicPeerKey::boxSubkey() const
{
  if (isNull())
    return PublicBoxKey();

  return PublicBoxKey(std::string(m_public, PublicSignKey::KeySize, PublicBoxKey::KeySize));
}

const NodeIdentifier &PublicPeerKey::nodeId() const
{
  if (isNull())
    return NodeIdentifier::INVALID;

  // Generate a node identifier and cache it
  if (m_nodeId.isNull()) {
    Botan::Pipe pipe(new Botan::Hash_Filter("SHA-512"));
    pipe.process_msg(m_public);
    m_nodeId = NodeIdentifier(pipe.read_all_as_string(0).substr(0, NodeIdentifier::length));
  }

  return m_nodeId;
}

std::string PublicPeerKey::signOpen(const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opSignOpen(m_public, 0, buffer);
}

void PrivatePeerKey::generate()
{
  if (isNull()) {
    m_public.resize(Public::KeySize);
    m_private.resize(KeySize);
  }

  opSignGenerate(m_public, 0, m_private, 0);
  opBoxGenerate(m_public, PublicSignKey::KeySize, m_private, PrivateSignKey::KeySize);
  m_nodeId = NodeIdentifier();
}

PrivateSignKey PrivatePeerKey::privateSignSubkey() const
{
  if (isNull())
    return PrivateSignKey();

  return PrivateSignKey(
    std::string(m_public, 0, PublicSignKey::KeySize),
    std::string(m_private, 0, PrivateSignKey::KeySize)
  );
}

PrivateBoxKey PrivatePeerKey::privateBoxSubkey() const
{
  if (isNull())
    return PrivateBoxKey();

  return PrivateBoxKey(
    std::string(m_public, PublicSignKey::KeySize, PublicBoxKey::KeySize),
    std::string(m_private, PrivateSignKey::KeySize, PrivateBoxKey::KeySize)
  );
}

std::string PrivatePeerKey::sign(const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opSign(m_private, 0, buffer);
}

std::string PrivatePeerKey::boxEncrypt(const PublicPeerKey &otherPublicKey,
                                       const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxEncrypt(otherPublicKey.raw(), PublicSignKey::KeySize, m_private, PrivateSignKey::KeySize, buffer);
}

std::string PrivatePeerKey::boxEncrypt(const PublicBoxKey &otherPublicKey,
                                       const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxEncrypt(otherPublicKey.raw(), 0, m_private, PrivateSignKey::KeySize, buffer);
}

std::string PrivatePeerKey::boxOpen(const PublicPeerKey &otherPublicKey,
                                    const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxOpen(otherPublicKey.raw(), PublicSignKey::KeySize, m_private, PrivateSignKey::KeySize, buffer);
}

std::string PrivatePeerKey::boxOpen(const PublicBoxKey &otherPublicKey,
                                    const std::string &buffer) const
{
  if (isNull())
    throw NullKey("Unable to perform operation on a null key!");

  return opBoxOpen(otherPublicKey.raw(), 0, m_private, PrivateSignKey::KeySize, buffer);
}

}
