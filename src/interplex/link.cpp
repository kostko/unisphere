/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2010 Jernej Kos <kostko@unimatrix-one.org>
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

namespace UniSphere {

Link::Link(LinkManager &manager, const NodeIdentifier &nodeId)
  : m_manager(manager),
    m_nodeId(nodeId),
    m_state(Link::State::Closed),
    m_dispatcher(new RoundRobinMessageDispatcher(m_linklets)),
    m_connectedLinklets(0),
    m_addressIterator(m_addressList.end()),
    m_retryTimer(manager.context().service())
{
}

Link::~Link()
{
  // Log link destruction
  UNISPHERE_LOG(m_manager.context(), Info, "Link: Destroying link.");
}

void Link::close()
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Close all linklets and then unregister this link
  BOOST_FOREACH(LinkletPtr &linklet, m_linklets) {
    linklet->close();
  }
  
  m_linklets.clear();
  m_connectedLinklets = 0;
  setState(Link::State::Closed);
  m_manager.remove(m_nodeId);
}

void Link::tryCleanup()
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex 
  LinkPtr self = shared_from_this();
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  if (m_linklets.size() == 0)
    close();
}

void Link::send(const Message &msg)
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  if (m_state != Link::State::Connected) {
    // FIXME Make this limit configurable
    if (m_messages.size() < 512)
      m_messages.push_back(msg);
    
    // TODO Trigger a reconnect when one is not in progress
  } else {
    m_dispatcher->send(msg);
  }
}

Contact Link::contact() const
{
  // TODO mutex
  Contact contact(m_nodeId);
  BOOST_FOREACH(const Address &address, m_addressList) {
    contact.addAddress(address);
  }
  return contact;
}

void Link::addLinklet(LinkletPtr linklet)
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // If the linklet already exists, ignore this request
  BOOST_FOREACH(LinkletPtr &l, m_linklets) {
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
      m_connectedLinklets++;
      setState(Link::State::Connected);
      break;
    }
    case Linklet::State::Connecting:
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
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Remove linklet from list of linklets
  m_linklets.remove(linklet);
  
  // Disconnect all signals
  linklet->signalConnectionSuccess.disconnect(boost::bind(&Link::linkletConnectionSuccess, this, _1));
  linklet->signalVerifyPeer.disconnect(boost::bind(&Link::linkletVerifyPeer, this, _1));
  linklet->signalConnectionFailed.disconnect(boost::bind(&Link::linkletConnectionFailed, this, _1));
  linklet->signalDisconnected.disconnect(boost::bind(&Link::linkletDisconnected, this, _1));
  linklet->signalMessageReceived.disconnect(boost::bind(&Link::linkletMessageReceived, this, _1, _2));
}

void Link::setState(State state)
{
  RecursiveUniqueLock lock(m_mutex);
  State old = m_state;
  m_state = state;
  
  // Handle state transitions
  if (old != Link::State::Connected && state == Link::State::Connected) {
    // Deliver queued messages
    if (!m_messages.empty()) {
      BOOST_FOREACH(Message &msg, m_messages) {
        m_dispatcher->send(msg);
      }
    }
    
    // Emit signal when we connect
    lock.unlock();
    signalEstablished(*this);
  } else if (old == Link::State::Connected && state != Link::State::Connected) {
    // Emit signal when we disconnect
    lock.unlock();
    signalDisconnected(*this);
  }
}

void Link::addContact(const Contact &contact, bool connect)
{
  // Transfer all contact addresses into the queue
  typedef std::pair<int, Address> AddressPair;
  BOOST_FOREACH(const AddressPair &p, contact.addresses()) {
    m_addressList.insert(p.second);
  }
  
  // If we are not yet connected, try connecting to some
  if (connect && m_state == Link::State::Closed && !m_addressList.empty())
    tryNextAddress();
}

void Link::tryNextAddress()
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex
  LinkPtr self = shared_from_this(); 
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Ensure that a connection actually still needs to be established
  if (m_connectedLinklets > 0)
    return;
  
  if (m_addressIterator == m_addressList.end() || ++m_addressIterator == m_addressList.end()) {
    signalCycledAddresses(*this);
    m_addressIterator = m_addressList.begin();
  }
  
  // When no further addresses are available, we close the link
  if (m_addressIterator == m_addressList.end()) {
    close();
    return;
  }
  
  // Log our attempt
  UNISPHERE_LOG(m_manager.context(), Info, "Link: Trying next address for outgoing connection.");
  
  const Address &address = *m_addressIterator;
  LinkletPtr linklet = m_manager.getLinkletFactory().create(address);
  addLinklet(linklet);
  linklet->connect(address);
}

void Link::linkletConnectionFailed(LinkletPtr linklet)
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Log connection failure
  UNISPHERE_LOG(m_manager.context(), Info, "Link: Outgoing connection failed. Queuing next try.");
  
  // Remove linklet from list of linklets and try next address
  removeLinklet(linklet);
  
  // TODO exponential backoff and limited number of retries
  if (!m_connectedLinklets) {
    m_retryTimer.expires_from_now(boost::posix_time::seconds(2));
    m_retryTimer.async_wait(boost::bind(&Link::tryNextAddress, this));
  }
}

bool Link::linkletVerifyPeer (LinkletPtr linklet)
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Check that the peer actually fits this link and close it if not
  if (linklet->peerContact().nodeId() != m_nodeId) {
    UNISPHERE_LOG(m_manager.context(), Error, "Link: Link identifier does not match destination node!");
    // TODO used contact address should be removed as it is clearly not valid
    // TODO also we should use some signal that would be used by Router to update routing
    //      table entries
    return false;
  }
  
  return true;
}
  
void Link::linkletConnectionSuccess(LinkletPtr linklet)
{
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Change link state as we now have at least one working linklet
  m_connectedLinklets++;
  setState(Link::State::Connected);
}

void Link::linkletDisconnected(LinkletPtr linklet)
{
  // Must take ownership of self, since close might be called while we
  // are still locking the mutex 
  LinkPtr self = shared_from_this();
  boost::lock_guard<boost::recursive_mutex> g(m_mutex);
  
  // Remove linklet from list of linklets
  removeLinklet(linklet);
  
  // Switch link state when we have no suitable linklets to use
  if (--m_connectedLinklets == 0) {
    setState(Link::State::Closed);
    
    // TODO This should only be called when we require a persistent connection
    tryNextAddress();
  }
}

void Link::linkletMessageReceived(LinkletPtr linklet, const Message &message)
{
  Message msg = message;
  msg.setOriginator(shared_from_this());
  signalMessageReceived(msg);
}

}
