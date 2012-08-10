/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#include "interplex/message.h"

#include <arpa/inet.h>

namespace UniSphere {

Message::Message()
  : m_type(Type::Null_Protocol),
    m_buffer(new std::vector<char>(Message::header_size))
{
  // Populate the header
  char *hdr = (char*) &buffer()[0];
  *((boost::uint8_t*) hdr) = static_cast<boost::uint8_t>(m_type);
  *((boost::uint32_t*) (hdr + 1)) = 0x00000000;
}

Message::Message(Type type, const google::protobuf::Message &msg)
  : m_type(type),
    m_buffer(new std::vector<char>())
{
  // Prepare buffer for header and payload
  size_t size = msg.ByteSize();
  m_buffer->resize(size + Message::header_size);
  
  // Serialize Protocol Buffers message into the payload
  msg.SerializeToArray(&buffer()[Message::header_size], size);
  
  // Populate the header
  char *hdr = (char*) &buffer()[0];
  *((boost::uint8_t*) hdr) = static_cast<boost::uint8_t>(m_type);
  *((boost::uint32_t*) (hdr + 1)) = htonl(size);
}

void Message::detach()
{
  // Create a new buffer thus releasing ownership of the previous one
  m_buffer = boost::shared_ptr<std::vector<char> >(new std::vector<char>(Message::header_size));
}

Message::Type Message::type() const
{
  return m_type;
}

std::vector<char> &Message::buffer()
{
  return *m_buffer;
}

std::vector<char> &Message::buffer() const
{
  return *m_buffer;
}

void Message::setOriginator(LinkPtr originator)
{
  m_originator = originator;
}

size_t Message::parseHeader()
{
  char *hdr = (char*) &buffer()[0];
  boost::uint8_t type = *((boost::uint8_t*) hdr);
  boost::uint32_t size = ntohl(*((boost::uint32_t*) (hdr + 1)));
  m_type = static_cast<Message::Type>(type);
  m_buffer->resize(Message::header_size + size);
  return size;
}

}
