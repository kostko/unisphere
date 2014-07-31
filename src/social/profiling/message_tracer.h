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
#ifndef UNISPHERE_SOCIAL_MESSAGETRACER_H
#define UNISPHERE_SOCIAL_MESSAGETRACER_H

#include "core/globals.h"

#include <map>
#include <boost/variant.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace UniSphere {

class RoutedMessage;

class UNISPHERE_EXPORT MessageTracer {
public:
  /**
   * Possible values that a record may hold.
   */
  using ValueType = boost::variant<
    bool,
    int,
    long,
    unsigned int,
    unsigned long,
    double,
    std::uint64_t,
    std::string,
    boost::posix_time::ptime
  >;

  /**
   * Convenience structure for simpler initialization of a record.
   */
  struct Element {
    /// Elemenet key
    std::string key;
    /// Element value
    ValueType value;
  };

  /// A single trace record contains multiple key-value pairs
  using Record = std::map<std::string, ValueType>;
  /// A map of records
  using RecordMap = std::map<std::string, Record>;

  /**
   * Class constructor.
   */
  MessageTracer();

  /**
   * Starts tracing messages. All previous traces are cleared.
   */
  void start();

  /**
   * Stops tracing messages.
   */
  void end();

  /**
   * Retrieves a trace record for the given message. If tracing is disabled
   * this will return a reference to a shared empty record -- this is why
   * one must always test if record is empty and not modify it in this case
   * as doing otherwise will cause memory corruption when called from multiple
   * threads.
   *
   * @param msg Routed message to trace
   * @return A mutable refrence to Record instance
   */
  Record &trace(const RoutedMessage &msg);

  /**
   * Returns a unique message identifier that can be used to track
   * the message over multiple hops.
   *
   * @param msg Routed message
   * @return Unique message identifier
   */
  std::string getMessageId(const RoutedMessage &msg);

  /**
   * Returns a map of collected trace records.
   */
  const RecordMap &getTraceRecords() const;
private:
  UNISPHERE_DECLARE_PRIVATE(MessageTracer)
};

}

#endif
