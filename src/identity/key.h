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
#include "identity/exceptions.h"

#include <botan/botan.h>

namespace UniSphere {

template <size_t PublicKeySize>
class UNISPHERE_EXPORT PublicKey {
public:
  /// Size of the public key
  static const size_t KeySize = PublicKeySize;

  /**
   * Format specifications for dealing with keys.
   */
  enum class Format {
    Raw,
    Base64
  };

  explicit PublicKey(const std::string &publicKey)
    : PublicKey(publicKey, Format::Raw)
  {
  }

  PublicKey(const std::string &publicKey, Format format)
    : m_public(PublicKeySize, 0)
  {
    convert<KeySize>(m_public, publicKey, format);
  }

  PublicKey() = default;
  PublicKey(const PublicKey&) = default;
  PublicKey &operator=(const PublicKey&) = default;
  PublicKey &operator=(const PublicKey&&) = delete;

  /**
   * Returns true if the key is null.
   */
  bool isNull() const { return m_public.empty(); }

  /**
   * Returns the public key as raw bytes.
   */
  const std::string &raw() const { return m_public; }

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
    pipe.process_msg(m_public);
    return pipe.read_all_as_string(0);
  }

  /**
   * Returns true if public keys are equal.
   */
  bool operator==(const PublicKey<PublicKeySize> &other) const
  {
    return m_public == other.m_public;
  }
protected:
  template <size_t BaseKeySize>
  void convert(std::string &data, const std::string &key, Format format) const
  {
    switch (format) {
      // Raw bytes format
      case Format::Raw: {
        if (key.size() != BaseKeySize)
          throw KeyDecodeFailed("Decoded key is not of the right size!");

        data = key;
        break;
      }

      // Base64 format
      case Format::Base64: {
        try {
          Botan::Pipe pipe(new Botan::Base64_Decoder());
          pipe.process_msg(key);
          const std::string &tmp = pipe.read_all_as_string(0);
          if (tmp.size() != BaseKeySize)
            throw KeyDecodeFailed("Decoded key is not of the right size!");

          data = tmp;
        } catch (std::invalid_argument &e) {
          throw KeyDecodeFailed("Error in key Base64 encoding!");
        }
        break;
      }
    }
  }
protected:
  /// Public key storage
  std::string m_public;
};

template <typename PublicKeyBase, size_t PrivateKeySize>
class UNISPHERE_EXPORT PrivateKey : public virtual PublicKey<PublicKeyBase::KeySize> {
public:
  /// Public key base type
  using Public = PublicKeyBase;
  /// Format specification for dealing with keys
  using Format = typename Public::Format;
  /// Size of the private key
  static const size_t KeySize = PrivateKeySize;
  /// Combined size of private and public keys
  static const size_t CombinedKeySize = PrivateKeySize + PublicKeyBase::KeySize;

  PrivateKey()
    : PublicKey<PublicKeyBase::KeySize>()
  {
  }

  PrivateKey(const std::string &publicKey, const std::string &privateKey)
    : PrivateKey(publicKey, privateKey, Format::Raw)
  {
  }

  PrivateKey(const std::string &publicKey, const std::string &privateKey, Format format)
    : PublicKey<PublicKeyBase::KeySize>(),
      m_private(PrivateKeySize, 0)
  {
    // We must replicate the convert call for public key part here because when
    // using virtual inheritance, only the default PublicKey constructor is called
    // even if the child class forwards constructors via "using" keyword
    this->m_public.resize(Public::KeySize);
    this->template convert<Public::KeySize>(this->m_public, publicKey, format);
    this->template convert<KeySize>(m_private, privateKey, format);
  }

  /**
   * Returns the private key as raw bytes.
   */
  const std::string &privateRaw() const { return m_private; }

  /**
   * Returns the private key as a base32 encoded string.
   */
  std::string privateBase32() const
  {
    // TODO
    return std::string();
  }

  /**
   * Returns the private key as a base64 encoded string.
   */
  std::string privateBase64() const
  {
    Botan::Pipe pipe(new Botan::Base64_Encoder());
    pipe.process_msg(m_private);
    return pipe.read_all_as_string(0);
  }

  /**
   * Returns a key with only the public key part.
   */
  PublicKeyBase publicKey() const
  {
    return PublicKeyBase(this->raw());
  }

  /**
   * Returns true if private keys are equal.
   */
  bool operator==(const PrivateKey<PublicKeyBase, PrivateKeySize> &other) const
  {
    return m_private == other.m_private;
  }
protected:
  /// Private key storage
  std::string m_private;
};

/**
 * Operator for private key serialization.
 */
template <typename T, size_t S>
inline UNISPHERE_EXPORT std::ostream &operator<<(std::ostream &stream, const PrivateKey<T, S> &key)
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
template <typename T, size_t S>
inline UNISPHERE_EXPORT std::istream &operator>>(std::istream &stream, PrivateKey<T, S> &key)
{
  std::string buffer(
    std::ceil(static_cast<float>((PrivateKey<T, S>::CombinedKeySize) * 4) / 3.0),
    0
  );
  stream.read((char*) &buffer[0], buffer.size());

  Botan::Pipe pipe(new Botan::Base64_Decoder());
  pipe.start_msg();
  pipe.write(buffer);
  pipe.end_msg();
  buffer = pipe.read_all_as_string(0);

  key = PrivateKey<T, S>(
    buffer.substr(0, PrivateKey<T, S>::Public::KeySize),
    buffer.substr(PrivateKey<T, S>::Public::KeySize, PrivateKey<T, S>::KeySize)
  );
  return stream;
}

}

#endif
