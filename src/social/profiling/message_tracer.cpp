/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "social/profiling/message_tracer.h"
#include "social/routed_message.h"

#include <botan/botan.h>

namespace UniSphere {

class MessageTracerPrivate {
public:
  MessageTracerPrivate();
public:
  /// Mutex
  std::mutex m_mutex;
  /// Current tracing state
  bool m_tracing;
  /// Saved records
  MessageTracer::RecordMap m_records;
  /// Null record when tracing is disabled
  MessageTracer::Record m_nullRecord;
};

MessageTracerPrivate::MessageTracerPrivate()
  : m_tracing(false)
{
}

MessageTracer::MessageTracer()
  : d(new MessageTracerPrivate)
{
}

std::string MessageTracer::getMessageId(const RoutedMessage &msg)
{
  std::uint32_t tmp;
  Botan::Pipe pipe(new Botan::Hash_Filter("MD5"), new Botan::Hex_Encoder);
  pipe.start_msg();
    pipe.write(msg.sourceNodeId().raw());
    tmp = msg.sourceCompId();
    pipe.write((Botan::byte*) &tmp, sizeof(std::uint32_t));
    pipe.write(msg.destinationNodeId().raw());
    tmp = msg.destinationCompId();
    pipe.write((Botan::byte*) &tmp, sizeof(std::uint32_t));
    tmp = msg.payloadType();
    pipe.write((Botan::byte*) &tmp, sizeof(std::uint32_t));
    pipe.write(msg.payload());
  pipe.end_msg();

  return pipe.read_all_as_string(0);
}

void MessageTracer::start()
{
  UniqueLock lock(d->m_mutex);
  d->m_tracing = true;
  d->m_records.clear();
}

void MessageTracer::end()
{
  UniqueLock lock(d->m_mutex);
  d->m_tracing = false;
}

MessageTracer::Record &MessageTracer::trace(const RoutedMessage &msg)
{
  UniqueLock lock(d->m_mutex);
  if (!d->m_tracing) {
    d->m_nullRecord.clear();
    return d->m_nullRecord;
  }

  Record &record = d->m_records[getMessageId(msg)];
  std::ostringstream tmp;
  record["timestamp"] = boost::posix_time::microsec_clock::universal_time();
  record["src"] = msg.sourceNodeId().hex();
  record["dst"] = msg.destinationNodeId().hex();

  tmp << msg.destinationAddress();
  record["dst_lr"] = tmp.str();
  return record;
}

const MessageTracer::RecordMap &MessageTracer::getTraceRecords() const
{
  return d->m_records;
}

}
