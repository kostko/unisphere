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
#ifndef UNISPHERE_INTERPLEX_MESSAGEDISPATCHER_H
#define UNISPHERE_INTERPLEX_MESSAGEDISPATCHER_H

#include "core/globals.h"
#include "interplex/message.h"
#include "interplex/linklet.h"

namespace UniSphere {

/**
 * A message dispatcher distributes messages going out via a single
 * link among potentially many linklets.
 */
class UNISPHERE_NO_EXPORT MessageDispatcher {
public:
  /**
   * Class constructor.
   *
   * @param linklets List of linklets to manage
   */
  MessageDispatcher(std::list<LinkletPtr> &linklets);
  
  /**
   * Class destructor.
   */
  virtual ~MessageDispatcher();

  /**
   * This method should be reimplemented by individual message dispatcher
   * implementations to deliver messages.
   *
   * @param msg Message to deliver
   */
  virtual void send(const Message &msg) = 0;
protected:
  /// The linklet list
  std::list<LinkletPtr> &m_linklets;
};

UNISPHERE_SHARED_POINTER(MessageDispatcher)

/**
 * A round-robin message dispatcher.
 */
class UNISPHERE_NO_EXPORT RoundRobinMessageDispatcher : public MessageDispatcher {
public:
  /**
   * Class constructor.
   *
   * @param linklets List of linklets to manage
   */
  RoundRobinMessageDispatcher(std::list<LinkletPtr> &linklets);
  
  /**
   * Delivers the message via some linklet.
   *
   * @param msg Message to deliver
   */
  void send(const Message &msg);
protected:
  /// Mutex protecting the dispatcher
  std::mutex m_mutex;
  /// Current linklet
  std::list<LinkletPtr>::iterator m_lastLinklet;
};

}

#endif
