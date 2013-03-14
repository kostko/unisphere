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
#include "interplex/message_dispatcher.h"

namespace UniSphere {

MessageDispatcher::MessageDispatcher(std::list<LinkletPtr> &linklets)
  : m_linklets(linklets)
{
}

MessageDispatcher::~MessageDispatcher()
{
}

RoundRobinMessageDispatcher::RoundRobinMessageDispatcher(std::list<LinkletPtr> &linklets)
  : MessageDispatcher(linklets),
    m_lastLinklet(linklets.end())
{
}

void RoundRobinMessageDispatcher::send(const Message &msg)
{
  UniqueLock lock(m_mutex);
  std::list<LinkletPtr>::iterator startLinklet = m_lastLinklet;
  do {
    if (m_lastLinklet == m_linklets.end())
      m_lastLinklet = m_linklets.begin();
    
    // No linklets to send to
    if (m_lastLinklet == m_linklets.end())
      return;
    
    // Only use connected linklets to send messages and abort when a
    // connected linklet is found or we have cycled around
    if ((*m_lastLinklet)->state() != Linklet::State::Connected) {
      m_lastLinklet++;
    } else {
      break;
    }
  } while (m_lastLinklet != startLinklet);
  
  LinkletPtr linklet = *m_lastLinklet;
  linklet->send(msg);
  m_lastLinklet++;
}

}
