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
#ifndef UNISPHERE_SOCIAL_MESSAGESNIFFER_H
#define UNISPHERE_SOCIAL_MESSAGESNIFFER_H

#include "core/globals.h"

#include <boost/signals2/signal.hpp>

namespace UniSphere {

class CompactRouter;
class RoutedMessage;

class UNISPHERE_EXPORT MessageSniffer {
public:
  using Filter = std::function<bool(const RoutedMessage&)>;

  /**
   * Constructs a message sniffer.
   */
  MessageSniffer();

  MessageSniffer(const MessageSniffer&) = delete;
  MessageSniffer &operator=(const MessageSniffer&) = delete;

  /**
   * Sets up the message filter. Only messages matching the filter will
   * be passed through.
   *
   * @param filter Filter functor
   */
  void setFilter(Filter filter);

  /**
   * Attaches a router instance to this sniffer. All messages from this
   * router will be processed. The router reference must remain valid
   * while the message sniffer is attached to the router.
   *
   * @param router A compact router instance
   */
  void attach(CompactRouter &router);

  /**
   * Detaches a router instance from this sniffer. Messages from this
   * router will no longer be processed.
   *
   * @param router A compact router instance
   */
  void detach(CompactRouter &router);

  /**
   * Starts sniffing messages.
   */
  void start();

  /**
   * Stops sniffing messages. All router instances are detached.
   */
  void stop();

  /**
   * Returns true if the sniffer is currently running.
   */
  bool isRunning() const;
public:
  /// Signal that gets emitted when a message is matched
  boost::signals2::signal<void(CompactRouter&, const RoutedMessage&)> signalMatchedMessage;
private:
  UNISPHERE_DECLARE_PRIVATE(MessageSniffer)
};

}

#endif
