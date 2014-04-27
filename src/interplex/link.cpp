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
#include "interplex/link.h"
#include "interplex/link_manager.h"
#include "interplex/linklet_factory.h"
#include "interplex/message_dispatcher.h"
#include "interplex/exceptions.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/make_shared.hpp>

namespace UniSphere {

Link::Link(LinkManager &manager, const PeerKey &peerKey, time_t maxIdleTime)
  : enable_shared_from_this<Link>(),
    m_manager(manager),
    m_logger(logging::keywords::channel = "link"),
    m_peerKey(peerKey),
    m_state(Link::State::Closed),
    m_maxIdleTime(maxIdleTime),
    m_dispatcher(boost::make_shared<RoundRobinMessageDispatcher>(m_linklets)),
    m_addressIterator(m_addressList.end()),
    m_retryTimer(manager.context().service()),
    m_idleTimer(manager.context().service())
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(manager.getLocalNodeId()));
}

void Link::init()
{
  m_idleTimer.expires_from_now(m_manager.context().roughly(m_maxIdleTime));
  m_idleTimer.async_wait(boost::bind(&Link::idleTimeout, shared_from_this(), _1));
}

Link::~Link()
{
  // Log link destruction
  BOOST_LOG_SEV(m_logger, log::normal) << "Destroying link.";
}

void Link::close()
{
  LinkPtr self = shared_from_this();
  RecursiveUniqueLock lock(m_mutex);

  // Move the link to invalid state where it cannot be used anymore
  if (m_state == Link::State::Invalid)
    return;
  m_state = Link::State::Invalid;

  BOOST_LOG_SEV(m_logger, log::normal) << "Closing link with " << m_peerKey.nodeId().hex() << ".";

  // Cancel timers
  m_retryTimer.cancel();
  m_idleTimer.cancel();

  // Close all linklets; we have to make a copy of this list because we
  // are going to modify the list
  std::list<LinkletPtr> linklets = m_linklets;
  for (LinkletPtr &linklet : linklets) {
    removeLinklet(linklet);
    linklet->close();
  }

  m_linklets.clear();
  m_manager.removeLink(shared_from_this());
}

void Link::tryCleanup()
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex
  LinkPtr self = shared_from_this();
  RecursiveUniqueLock lock(m_mutex);

  if (m_linklets.size() == 0)
    close();
}

void Link::send(const Message &msg)
{
  RecursiveUniqueLock lock(m_mutex);

  // When messages are queued to an invalid link, they are lost as the link
  // is going to be deleted in a moment
  if (m_state == Link::State::Invalid)
    return;

  if (m_state != Link::State::Connected) {
    // FIXME Make this limit configurable
    m_messages.push_back(msg);
    if (m_messages.size() > 512)
      m_messages.pop_front();

    // Trigger a reconnect when one is not in progress
    if (m_state != Link::State::Connecting)
      tryNextAddress();
  } else {
    m_dispatcher->send(msg);
  }
}

Contact Link::contact()
{
  RecursiveUniqueLock lock(m_mutex);
  Contact contact(m_peerKey);
  for (const Address &address : m_addressList) {
    contact.addAddress(address);
  }
  return contact;
}

void Link::addLinklet(LinkletPtr linklet)
{
  RecursiveUniqueLock lock(m_mutex);

  // If the linklet already exists, ignore this request
  for (LinkletPtr &l : m_linklets) {
    if (l == linklet)
      return;
  }

  // Limit the number of acceptable linklets
  // TODO Make this limit configurable
  if (m_linklets.size() > 16)
    throw TooManyLinklets();

  m_linklets.push_back(linklet);

  switch (linklet->state()) {
    case Linklet::State::Connected: {
      // A connected linklet; might change link state
      setState(Link::State::Connected);
      break;
    }
    case Linklet::State::IntroWait:
    case Linklet::State::Connecting: {
      if (m_state == Link::State::Closed)
        setState(Link::State::Connecting);

      // Don't break here, so we connect the signal below!
    }
    case Linklet::State::Closed: {
      // Connection is pending, we should connect to its success routine
      linklet->signalConnectionSuccess.connect(boost::bind(&Link::linkletConnectionSuccess, this, _1));
      break;
    }
    case Linklet::State::Listening: break;
  }

  // Connect to linklet signals
  linklet->signalVerifyPeer.connect(boost::bind(&Link::linkletVerifyPeer, this, _1));
  linklet->signalConnectionFailed.connect(boost::bind(&Link::linkletConnectionFailed, this, _1));
  linklet->signalDisconnected.connect(boost::bind(&Link::linkletDisconnected, this, _1));
  linklet->signalMessageReceived.connect(boost::bind(&Link::linkletMessageReceived, this, _1, _2));
}

void Link::removeLinklet(LinkletPtr linklet)
{
  RecursiveUniqueLock lock(m_mutex);

  // Remove linklet from list of linklets
  m_linklets.remove(linklet);
  m_dispatcher->reset();

  // Disconnect all signals
  linklet->signalConnectionSuccess.disconnect(boost::bind(&Link::linkletConnectionSuccess, this, _1));
  linklet->signalVerifyPeer.disconnect(boost::bind(&Link::linkletVerifyPeer, this, _1));
  linklet->signalConnectionFailed.disconnect(boost::bind(&Link::linkletConnectionFailed, this, _1));
  linklet->signalDisconnected.disconnect(boost::bind(&Link::linkletDisconnected, this, _1));
  linklet->signalMessageReceived.disconnect(boost::bind(&Link::linkletMessageReceived, this, _1, _2));

  // Check that we have the proper state
  checkLinkletState();
}

void Link::checkLinkletState()
{
  RecursiveUniqueLock lock(m_mutex);
  if (m_state == Link::State::Invalid)
    return;

  bool connected = false;
  bool connecting = false;
  for (LinkletPtr &l : m_linklets) {
    if (l->state() == Linklet::State::Connected) {
      connected = true;
    } else if (l->state() == Linklet::State::Connecting || l->state() == Linklet::State::IntroWait) {
      connecting = true;
    }
  }

  if (connected) {
    setState(Link::State::Connected);
  } else if (connecting) {
    setState(Link::State::Connecting);
  } else {
    setState(Link::State::Closed);
  }
}

void Link::setState(State state)
{
  RecursiveUniqueLock lock(m_mutex);
  BOOST_ASSERT(m_state != Link::State::Invalid);

  State old = m_state;
  m_state = state;

  // Handle state transitions
  if (old != Link::State::Connected && state == Link::State::Connected) {
    // Deliver queued messages
    if (!m_messages.empty()) {
      for (Message &msg : m_messages) {
        m_dispatcher->send(msg);
      }
      m_messages.clear();
    }
  }
}

void Link::addContact(const Contact &contact)
{
  RecursiveUniqueLock lock(m_mutex);
  BOOST_ASSERT(contact.peerKey() == m_peerKey);

  // Transfer all contact addresses into the queue
  for (const auto &p : contact.addresses()) {
    m_addressList.insert(p.second);
  }
}

void Link::tryNextAddress()
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex
  LinkPtr self = shared_from_this();
  RecursiveUniqueLock lock(m_mutex);

  // Ensure that a connection actually still needs to be established
  if (m_state != Link::State::Closed)
    return;

  if (m_addressIterator == m_addressList.end() || ++m_addressIterator == m_addressList.end()) {
    m_addressIterator = m_addressList.begin();
  }

  // When no further addresses are available, we close the link
  if (m_addressIterator == m_addressList.end()) {
    close();
    return;
  }

  // Log our attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Trying next address for outgoing connection with "
    << m_peerKey.nodeId().hex() << ".";

  // Change state to connecting
  setState(Link::State::Connecting);

  const Address &address = *m_addressIterator;
  LinkletPtr linklet = m_manager.getLinkletFactory().create(address);
  addLinklet(linklet);
  linklet->connect(address);
}

void Link::linkletConnectionFailed(LinkletPtr linklet)
{
  RecursiveUniqueLock lock(m_mutex);

  // Log connection failure
  BOOST_LOG_SEV(m_logger, log::normal) << "Outgoing connection failed. Queuing next try.";

  // Remove linklet from list of linklets and try next address
  removeLinklet(linklet);

  // TODO exponential backoff and limited number of retries
  if (m_state == Link::State::Closed) {
    m_retryTimer.expires_from_now(boost::posix_time::seconds(2));
    m_retryTimer.async_wait(boost::bind(&Link::retryTimeout, shared_from_this(), _1));
  }
}

bool Link::linkletVerifyPeer(LinkletPtr linklet)
{
  RecursiveUniqueLock lock(m_mutex);

  // Check that the peer actually fits this link and close it if not
  if (linklet->peerContact().nodeId() != m_peerKey.nodeId()) {
    BOOST_LOG_SEV(m_logger, log::error) << "Link identifier does not match destination node! " <<
      linklet->peerContact().nodeId().hex() << " -- " << m_peerKey.nodeId().hex();
    // TODO used contact address should be removed as it is clearly not valid
    // TODO also we should use some signal that would be used by Router to update routing
    //      table entries
    return false;
  }

  return true;
}

void Link::linkletConnectionSuccess(LinkletPtr linklet)
{
  RecursiveUniqueLock lock(m_mutex);
  checkLinkletState();
}

void Link::linkletDisconnected(LinkletPtr linklet)
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex
  LinkPtr self = shared_from_this();
  RecursiveUniqueLock lock(m_mutex);

  // Remove linklet from list of linklets and close the link when there are no
  // more linklets to be seen (link has changed state to Closed)
  removeLinklet(linklet);
  if (m_state == Link::State::Closed) {
    close();
  }
}

void Link::linkletMessageReceived(LinkletPtr linklet, const Message &message)
{
  // Reschedule the idle timer for another interval
  {
    RecursiveUniqueLock lock(m_mutex);
    if (m_idleTimer.expires_from_now(m_manager.context().roughly(m_maxIdleTime)) > 0) {
      m_idleTimer.async_wait(boost::bind(&Link::idleTimeout, shared_from_this(), _1));
    }
  }

  Message msg = message;
  msg.setOriginator(m_peerKey.nodeId());
  signalMessageReceived(msg);
}

void Link::retryTimeout(const boost::system::error_code &error)
{
  // Ignore aborted timer operations
  if (error == boost::asio::error::operation_aborted)
    return;

  tryNextAddress();
}

void Link::idleTimeout(const boost::system::error_code &error)
{
  // Ignore aborted timer operations
  if (error == boost::asio::error::operation_aborted)
    return;

  BOOST_LOG_SEV(m_logger, log::normal) << "Timeout.";
  close();
}

}
