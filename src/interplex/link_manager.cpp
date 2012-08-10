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

void LinkManager::setListenLinkInit(std::function<void(Link&)> init)
{
  m_listeningInit = init;
}

Link& LinkManager::connect(const Contact &contact, std::function<void(Link&)> init)
{
  return create(contact, init, true);
}

Link &LinkManager::create(const Contact &contact, std::function<void(Link&)> init, bool connect)
{
  UpgradableLock lock(m_linksMutex);
  LinkPtr link;
  
  // Find or create a link
  if (m_links.find(contact.nodeId()) != m_links.end()) {
    link = m_links[contact.nodeId()];
  } else {
    link = LinkPtr(new Link(*this, contact.nodeId()));
    if (init) {
      init(*link);
    }
    
    UpgradeToUniqueLock unique(lock);
    m_links[contact.nodeId()] = link;
  }
  
  // Setup link contact
  link->addContact(contact, connect);
  return *link;
}

void LinkManager::listen(const Contact &contact)
{
  UpgradableLock lock(m_listenersMutex);
  
  typedef std::pair<int, Address> PriorityAddress;
  BOOST_FOREACH(PriorityAddress it, contact.addresses()) {
    try {
      LinkletPtr linklet = m_linkletFactory.create(it.second);
      linklet->signalAcceptedConnection.connect(boost::bind(&LinkManager::linkletAcceptedConnection, this, _1));
      linklet->listen(it.second);
      
      UpgradeToUniqueLock unique(lock);
      m_listeners.push_back(linklet);
    } catch (LinkletListenFailed &e) {
      // Failed to listen on specified address, skip this one
    }
  }
}

void LinkManager::listen(const Address &address, const NodeIdentifier &nodeId)
{
  Contact contact(nodeId);
  contact.addAddress(address);
  listen(contact);
}

void LinkManager::remove(const NodeIdentifier &nodeId)
{
  UpgradableLock lock(m_linksMutex);
  
  if (m_links.find(nodeId) == m_links.end())
    return;
  
  LinkPtr link = m_links[nodeId];
  if (link->state() != Link::State::Closed)
    return;
  
  UpgradeToUniqueLock unique(lock);
  m_links.erase(nodeId);
}

void LinkManager::linkletAcceptedConnection(LinkletPtr linklet)
{
  // Create and register a new link from the given linklet
  Link &link = create(linklet->peerContact(), m_listeningInit, false);
  
  try {
    link.addLinklet(linklet);
  } catch (TooManyLinklets &e) {
    link.tryCleanup();
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
