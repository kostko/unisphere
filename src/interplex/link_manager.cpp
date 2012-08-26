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
#include "interplex/link_manager.h"
#include "interplex/linklet.h"
#include "interplex/link.h"
#include "interplex/host_introspector.h"
#include "interplex/exceptions.h"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace UniSphere {

LinkManager::LinkManager(Context &context, const NodeIdentifier &nodeId)
  : m_context(context),
    m_nodeId(nodeId),
    m_linkletFactory(*this)
{
}

void LinkManager::setLinkInitializer(std::function<void(Link&)> init)
{
  m_linkInitializer = init;
}

LinkPtr LinkManager::connect(const Contact &contact)
{
  return create(contact, true);
}

LinkPtr LinkManager::create(const Contact &contact, bool connect)
{
  UniqueLock lock(m_linksMutex);
  LinkPtr link;
  
  // Find or create a link
  if (m_links.find(contact.nodeId()) != m_links.end()) {
    link = m_links[contact.nodeId()];
  } else {
    link = LinkPtr(new Link(*this, contact.nodeId()));
    if (m_linkInitializer) {
      m_linkInitializer(*link);
    }
    
    m_links[contact.nodeId()] = link;
  }
  
  // Setup link contact
  link->addContact(contact, connect);
  return link;
}

bool LinkManager::listen(const Address &address)
{
  UniqueLock lock(m_listenersMutex);
  try {
    LinkletPtr linklet = m_linkletFactory.create(address);
    linklet->signalAcceptedConnection.connect(boost::bind(&LinkManager::linkletAcceptedConnection, this, _1));
    linklet->listen(address);
    m_listeners.push_back(linklet);
  } catch (LinkletListenFailed &e) {
    // Failed to listen on specified address
    return false;
  }
  
  return true;
}

void LinkManager::remove(const NodeIdentifier &nodeId)
{
  UniqueLock lock(m_linksMutex);
  
  if (m_links.find(nodeId) == m_links.end())
    return;
  
  LinkPtr link = m_links[nodeId];
  if (link->state() != Link::State::Closed)
    return;
  
  m_links.erase(nodeId);
}

LinkPtr LinkManager::getLink(const NodeIdentifier &nodeId)
{
  if (m_links.find(nodeId) == m_links.end())
    return LinkPtr();
  
  return m_links[nodeId];
}

void LinkManager::linkletAcceptedConnection(LinkletPtr linklet)
{
  // Create and register a new link from the given linklet
  LinkPtr link = create(linklet->peerContact(), false);
  
  try {
    link->addLinklet(linklet);
  } catch (TooManyLinklets &e) {
    link->tryCleanup();
    linklet->close();
  }
}

Contact LinkManager::getLocalContact() const
{
  // TODO This should probably be cached
  Contact contact(m_nodeId);
  BOOST_FOREACH(LinkletPtr linklet, m_listeners) {
    Address address = linklet->address();
    if (address.type() == Address::Type::IP) {
      auto endpoint = address.toIpEndpoint();
      auto ip = endpoint.address();
      
      if ((ip.is_v4() && ip.to_v4() == boost::asio::ip::address_v4::any()) ||
        (ip.is_v6() && ip.to_v6() == boost::asio::ip::address_v6::any())) {
        // Listen on any interface, we need the introspector to discover all
        // available addresses
        Contact lcontact = HostIntrospector::localContact(endpoint.port());
        BOOST_FOREACH(const auto &laddr, lcontact.addresses()) {
          contact.addAddress(laddr.second);
        }
      } else {
        // We have address and port, simply add to contact
        contact.addAddress(address);
      }
    } else {
      // Other types of addressing (non-IP), just add for now
      contact.addAddress(address);
    }
  }
  
  return contact;
}

  
}
