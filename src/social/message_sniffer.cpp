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
#include "social/message_sniffer.h"
#include "social/routed_message.h"
#include "social/compact_router.h"
#include "social/social_identity.h"
#include "core/operators.h"

namespace UniSphere {

class MessageSnifferPrivate {
public:
  MessageSnifferPrivate(MessageSniffer &sniffer);

  bool handleMessage(CompactRouter &router, const RoutedMessage &msg);
public:
  UNISPHERE_DECLARE_PUBLIC(MessageSniffer)

  /// Mutex protecting this datastructure
  mutable std::recursive_mutex m_mutex;
  /// Sniffer state
  bool m_running;
  /// Currently installed message filter
  MessageSniffer::Filter m_filter;
  /// Router attachments
  std::unordered_map<NodeIdentifier, std::list<boost::signals2::connection>> m_attachments;
};

MessageSnifferPrivate::MessageSnifferPrivate(MessageSniffer &sniffer)
  : q(sniffer),
    m_running(false)
{
}

bool MessageSnifferPrivate::handleMessage(CompactRouter &router, const RoutedMessage &msg)
{
  if (!m_running)
    return true;

  if (!m_filter || m_filter(msg))
    q.signalMatchedMessage(router, msg);

  return true;
}

MessageSniffer::MessageSniffer()
  : d(new MessageSnifferPrivate(*this))
{
}

void MessageSniffer::setFilter(Filter filter)
{
  d->m_filter = filter;
}

void MessageSniffer::attach(CompactRouter &router)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_attachments[router.identity().localId()]
    << router.signalDeliverMessage.connect(boost::bind(&MessageSnifferPrivate::handleMessage, d, std::ref(router), _1))
    << router.signalForwardMessage.connect(boost::bind(&MessageSnifferPrivate::handleMessage, d, std::ref(router), _1))
  ;
}

void MessageSniffer::detach(CompactRouter &router)
{
  RecursiveUniqueLock lock(d->m_mutex);
  auto it = d->m_attachments.find(router.identity().localId());
  if (it == d->m_attachments.end())
    return;

  for (auto &c : it->second)
    c.disconnect();
  d->m_attachments.erase(it);
}

void MessageSniffer::start()
{
  d->m_running = true;
}

void MessageSniffer::stop()
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_running = false;
  for (auto &p : d->m_attachments) {
    for (auto &c : p.second)
      c.disconnect();
  }
  d->m_attachments.clear();
}

bool MessageSniffer::isRunning() const
{
  return d->m_running;
}

}
