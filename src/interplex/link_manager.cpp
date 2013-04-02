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
#ifdef UNISPHERE_DEBUG
    ,
    m_measure(nodeId.as(NodeIdentifier::Format::Hex))
#endif
{
}

void LinkManager::setLocalAddress(const Address &address)
{
  m_localAddress = address;
}

void LinkManager::send(const Contact &contact, const Message &msg)
{
  RecursiveUniqueLock lock(m_linksMutex);
  BOOST_ASSERT(!contact.isNull());
  
  // Ignore attempted deliveries to the local node
  if (contact.nodeId() == m_nodeId)
    return;
  
  // Create a new link or retrieve an existing link if one exists
  LinkPtr link = get(contact);
  if (!link) {
    // No contact address is available and link is not an existing one; we
    // can only drop the packet
    UNISPHERE_LOG(*this, Warning, "LinkManager: No link to destination, dropping message!");
    return;
  }
  
  link->send(msg);
}

void LinkManager::send(const NodeIdentifier &nodeId, const Message &msg)
{
  send(Contact(nodeId), msg);
}

LinkPtr LinkManager::get(const Contact &contact, bool create)
{
  RecursiveUniqueLock lock(m_linksMutex);
  LinkPtr link;
  if (m_links.find(contact.nodeId()) != m_links.end()) {
    link = m_links[contact.nodeId()];
    
    // It can happen that link has switched to invalid and we really should not
    // queue messages to such a link as they will be lost
    if (!link->isValid()) {
      remove(link);
      link = LinkPtr();
    }
  }
  
  if (!link) {
    if (contact.hasAddresses() && create) {
      link = LinkPtr(new Link(*this, contact.nodeId(), 600));
      link->init();
      link->signalMessageReceived.connect(boost::bind(&LinkManager::linkMessageReceived, this, _1));
      m_links[contact.nodeId()] = link;
    } else {
      // No contact address is available (or create not allowed) and link is not an existing one
      return LinkPtr();
    }
  }
  
  if (contact.hasAddresses())
    link->addContact(contact);
  
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

void LinkManager::close()
{
  // First shut down listeners
  {
    UniqueLock lock(m_listenersMutex);
    for (LinkletPtr linklet : m_listeners) {
      linklet->close();
    }
    m_listeners.clear();
  }
  
  // Then shut down all links
  {
    RecursiveUniqueLock lock(m_linksMutex);
    // Make a copy since we will be modifying the original links list
    auto links = m_links;
    for (auto p : links) {
      p.second->close();
    }
  }
}

Contact LinkManager::getLinkContact(const NodeIdentifier &linkId)
{
  RecursiveUniqueLock lock(m_linksMutex);
  if (m_links.find(linkId) == m_links.end())
    return Contact();
  
  return m_links[linkId]->contact();
}

void LinkManager::remove(LinkPtr link)
{
  RecursiveUniqueLock lock(m_linksMutex);
  if (m_links.find(link->nodeId()) == m_links.end() ||
      m_links[link->nodeId()] != link)
    return;
  
  link->signalMessageReceived.disconnect(boost::bind(&LinkManager::linkMessageReceived, this, _1));
  m_links.erase(link->nodeId());
}

std::list<NodeIdentifier> LinkManager::getLinkIds()
{
  RecursiveUniqueLock lock(m_linksMutex);
  std::list<NodeIdentifier> links;
  for (auto p : m_links) {
    links.push_back(p.first);
  }
  return links;
}

void LinkManager::linkMessageReceived(const Message &msg)
{
  signalMessageReceived(msg);
}

bool LinkManager::verifyPeer(const Contact &contact)
{
  // If peer has the same identifier as the local node, we should drop the link
  if (contact.nodeId() == getLocalNodeId()) {
    UNISPHERE_LOG(*this, Warning, "LinkManager: Attempted nodeId collision, refusing link.");
    return false;
  }

  // Invoke externally registered verification hooks
  return signalVerifyPeer(contact);
}

void LinkManager::linkletAcceptedConnection(LinkletPtr linklet)
{
  // Create and register a new link from the given linklet
  LinkPtr link = get(linklet->peerContact());
  
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
