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
#ifndef UNISPHERE_IDENTITY_KEY_H
#define UNISPHERE_IDENTITY_KEY_H

#include "core/globals.h"

#include <botan/botan.h>

namespace UniSphere {

/// A secure vector for storing key data
typedef Botan::SecureVector<unsigned char> KeyData;

/**
 * Copies insecure string data into a secure KeyData vector.
 *
 * @param data Source data
 * @return KeyData
 */
inline UNISPHERE_EXPORT KeyData fromInsecureKeyStorage(const std::string &data)
{
  return KeyData((unsigned char*) data.data(), data.size());
}

template <typename PublicBase>
class UNISPHERE_EXPORT Key : public PublicBase {
public:
  /**
   * Format specifications for dealing with keys.
   */
  enum class Format {
    Raw,
    Base64
  };

  /**
   * Constructs a null key.
   */
  Key()
    : PublicBase()
  {
  }

  /**
   * Constructs a new key instance from a raw public key buffer.
   *
   * @param publicKey Raw public key buffer
   */
  explicit Key(const std::string &publicKey)
    : Key(publicKey, Format::Raw)
  {
  }

  /**
   * Constructs a new key instance.
   *
   * @param publicKey Public key in the specified format
   * @param format Key format
   */
  Key(const std::string &publicKey, Format format)
    : PublicBase()
  {
    switch (format) {
      // Raw bytes format
      case Format::Raw: this->m_public = publicKey; break;

      // Base64 format
      case Format::Base64: {
        try {
          Botan::Pipe pipe(new Botan::Base64_Decoder());
          pipe.process_msg(publicKey);
          this->m_public = pipe.read_all_as_string(0);
        } catch (std::exception &e) {
          this->m_public.clear();
        }
        break;
      }
    }

    this->validatePublic();
  }

  /**
   * Returns true if the key is a null key.
   */
  inline bool isNull() const { return this->m_public.empty(); }

  /**
   * Returns the public key as raw bytes.
   */
  inline std::string raw() const { return this->m_public; };

  /**
   * Returns the public key as a base32 encoded string.
   */
  std::string base32() const
  {
    // TODO
    return std::string();
  }

  /**
   * Returns the public key as a base64 encoded string.
   */
  std::string base64() const
  {
    Botan::Pipe pipe(new Botan::Base64_Encoder());
    pipe.process_msg(this->m_public);
    return pipe.read_all_as_string(0);
  }

  /**
   * Returns true if public keys are equal.
   */
  bool operator==(const Key &other) const
  {
    return this->m_public == other.m_public;
  }
};

template <typename PrivateBase>
class UNISPHERE_EXPORT PrivateKey : public PrivateBase {
public:
  typedef typename PrivateBase::Format Format;

  PrivateKey()
    : PrivateBase()
  {}

  PrivateKey(const std::string &publicKey,
             const KeyData &privateKey)
    : PrivateKey(publicKey, privateKey, Format::Raw)
  {}

  PrivateKey(const std::string &publicKey,
             const KeyData &privateKey,
             Format format)
    : PrivateBase(publicKey, format)
  {
    switch (format) {
      // Raw bytes format
      case Format::Raw: this->m_private = privateKey; break;

      // Base64 format
      case Format::Base64: {
        try {
          Botan::Pipe pipe(new Botan::Base64_Decoder());
          pipe.process_msg(privateKey);
          this->m_private = pipe.read_all(0);
        } catch (std::exception &e) {
          this->m_private.clear();
        }
        break;
      }
    }

    this->validatePrivate();
  }

  /**
   * Returns the private key as raw bytes.
   */
  inline KeyData privateRaw() const { return this->m_private; };

  /**
   * Returns the private key as a base32 encoded string.
   */
  KeyData privateBase32() const
  {
    // TODO
    return KeyData();
  }

  /**
   * Returns the private key as a base64 encoded string.
   */
  KeyData privateBase64() const
  {
    Botan::Pipe pipe(new Botan::Base64_Encoder());
    pipe.process_msg(this->m_private);
    return pipe.read_all(0);
  }

  /**
   * Returns a peer key with only the public key part.
   *
   * @throws NullPeerKey When attempting to call with a null key
   */
  typename PrivateBase::public_key_type publicKey() const
  {
    return (typename PrivateBase::public_key_type)(this->m_public);
  }

  /**
   * Returns true if private peer keys are equal.
   */
  bool operator==(const PrivateKey<PrivateBase> &other) const
  {
    return this->m_private == other.m_private;
  }
};


/**
 * Operator for private key serialization.
 */
template <typename T>
inline UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &stream, const PrivateKey<T> &key)
{
  Botan::Pipe pipe(new Botan::Base64_Encoder());
  pipe.start_msg();
  pipe.write(key.raw());
  pipe.write(key.privateRaw());
  pipe.end_msg();
  stream << pipe;

  return stream;
}

/**
 * Operator for private key deserialization.
 */
template <typename T>
inline UNISPHERE_EXPORT std::istream &operator>>(std::istream &stream, PrivateKey<T> &key)
{
  KeyData buffer(
    std::ceil(static_cast<float>((PrivateKey<T>::public_key_length + PrivateKey<T>::private_key_length) * 4) / 3.0)
  );
  stream.read((char*) &buffer[0], buffer.size());

  Botan::Pipe pipe(new Botan::Base64_Decoder());
  pipe.start_msg();
  pipe.write(buffer);
  pipe.end_msg();
  buffer = pipe.read_all();

  // Extract public key
  std::string publicKey((char*) &buffer[0], PrivateKey<T>::public_key_length);

  // Extract private key
  KeyData privateKey(PrivateKey<T>::private_key_length);
  privateKey.copy(&buffer[PrivateKey<T>::public_key_length], PrivateKey<T>::private_key_length);

  key = PrivateKey<T>(publicKey, privateKey);
  return stream;
}

}

#endif
