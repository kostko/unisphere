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
#include "interplex/link_manager.h"
#include "interplex/linklet.h"
#include "interplex/link.h"
#include "interplex/host_introspector.h"
#include "interplex/exceptions.h"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace UniSphere {

class LinkManagerPrivate {
public:
  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   * @param privateKey Node private key
   * @param manager Public manager reference
   */
  LinkManagerPrivate(Context &context,
                     const PrivatePeerKey &privateKey,
                     LinkManager &manager);

  /**
   * Sends a message to the given contact address.
   *
   * @param contact Destination node's contact information
   * @param msg Message to send
   */
  void send(const Contact &contact, const Message &msg);

  /**
   * Sends a message to the given peer. If there is no existing link
   * for the specified peer the message will not be delivered.
   *
   * @param nodeId Destination node's identifier
   * @param msg Message to send
   */
  void send(const NodeIdentifier &nodeId, const Message &msg);

  /**
   * Opens a listening linklet.
   *
   * @param address Local address to listen on
   * @return True if listen was successful
   */
  bool listen(const Address &address);

  /**
   * Closes all existing links and stops listening for any new ones.
   */
  void close();

  /**
   * Returns the contact for a given link identifier.
   *
   * @param linkId Link identifier
   * @return Contact for the specified link
   */
  Contact getLinkContact(const NodeIdentifier &linkId);

  /**
   * Returns a list of node identifiers to links that we have established.
   */
  std::list<NodeIdentifier> getLinkIds();

  /**
   * Returns the local contact information.
   */
  Contact getLocalContact() const;

  /**
   * Invokes registered peer verification hooks (signalVerifyPeer) and
   * returns the result.
   *
   * @param contact Contact to verify
   * @return True if verification was successful, false otherwise
   */
  bool verifyPeer(const Contact &contact);

  /**
   * Returns a link suitable for communication with the specified contact.
   *
   * @param contact Contact information
   * @return A valid link instance or null
   */
  LinkPtr getOrCreateLink(const Contact &contact);

  /**
   * Removes a specific link.
   *
   * @param link Link instance
   */
  void removeLink(LinkPtr link);

  /**
   * Called by a listener @ref Linklet when a new connection gets accepted
   * and is ready for dispatch.
   *
   * @param linklet New linklet for this connection
   */
  void linkletAcceptedConnection(LinkletPtr linklet);

  /**
   * Called by a @ref Link when a new message is received.
   *
   * @param msg Received message
   */
  void linkMessageReceived(const Message &msg);
public:
  UNISPHERE_DECLARE_PUBLIC(LinkManager)

  /// UNISPHERE context this manager belongs to
  Context &m_context;
  /// Logger instance
  Logger m_logger;
  /// Local private key
  PrivatePeerKey m_privateKey;
  /// Linklet factory for producing new linklets
  LinkletFactory m_linkletFactory;
  /// Mapping of all managed links by their identifiers
  std::unordered_map<NodeIdentifier, LinkPtr> m_links;
  /// Mutex protecting the link mapping
  std::recursive_mutex m_linksMutex;
  /// A list of all listening linklets
  std::list<LinkletPtr> m_listeners;
  /// Mutex protecting the listening linklet list
  std::mutex m_listenersMutex;
  /// Local outgoing address
  Address m_localAddress;
  /// Statistics
  LinkManager::Statistics m_statistics;
};

LinkManagerPrivate::LinkManagerPrivate(Context &context,
                                       const PrivatePeerKey &privateKey,
                                       LinkManager &manager)
  : q(manager),
    m_context(context),
    m_logger(logging::keywords::channel = "link_manager"),
    m_privateKey(privateKey),
    m_linkletFactory(manager)
{
}

LinkPtr LinkManagerPrivate::getOrCreateLink(const Contact &contact)
{
  RecursiveUniqueLock lock(m_linksMutex);
  LinkPtr link;
  auto it = m_links.find(contact.nodeId());
  if (it != m_links.end()) {
    link = it->second;

    // It can happen that link has switched to invalid and we really should not
    // queue messages to such a link as they will be lost
    if (!link->isValid()) {
      removeLink(link);
      link = LinkPtr();
    }
  }

  if (!link) {
    if (contact.hasAddresses()) {
      // Note that we can't use make_shared here because the Link constructor is private
      link = LinkPtr(new Link(q, contact.peerKey(), 600));
      link->init();
      link->signalMessageReceived.connect(boost::bind(&LinkManagerPrivate::linkMessageReceived, this, _1));
      m_links.insert({{ contact.nodeId(), link }});
      link->addContact(contact);
    } else {
      // No contact address is available (or create not allowed) and link is not an existing one
      return LinkPtr();
    }
  }

  return link;
}

void LinkManagerPrivate::send(const Contact &contact, const Message &msg)
{
  RecursiveUniqueLock lock(m_linksMutex);
  BOOST_ASSERT(!contact.isNull());

  // Ignore attempted deliveries to the local node
  if (contact.nodeId() == m_privateKey.nodeId())
    return;

  // Create a new link or retrieve an existing link if one exists
  LinkPtr link = getOrCreateLink(contact);
  if (!link) {
    // No contact address is available and link is not an existing one; we
    // can only drop the packet
    BOOST_LOG_SEV(m_logger, log::warning) << "No link to destination, dropping message!";
    return;
  }

  link->send(msg);
  m_statistics.global.msgXmits++;
  m_statistics.links[link->nodeId()].msgXmits++;
}

void LinkManagerPrivate::send(const NodeIdentifier &nodeId, const Message &msg)
{
  RecursiveUniqueLock lock(m_linksMutex);
  LinkPtr link;
  auto it = m_links.find(nodeId);
  if (it != m_links.end()) {
    link = it->second;

    // It can happen that link has switched to invalid and we really should not
    // queue messages to such a link as they will be lost
    if (!link->isValid()) {
      BOOST_LOG_SEV(m_logger, log::warning) << "No link to destination, dropping message!";
      return removeLink(link);
    }
  } else {
    BOOST_LOG_SEV(m_logger, log::warning) << "No link to destination, dropping message!";
    return;
  }

  link->send(msg);
  m_statistics.global.msgXmits++;
  m_statistics.links[link->nodeId()].msgXmits++;
}

bool LinkManagerPrivate::listen(const Address &address)
{
  UniqueLock lock(m_listenersMutex);
  try {
    LinkletPtr linklet = m_linkletFactory.create(address);
    linklet->signalAcceptedConnection.connect(boost::bind(&LinkManagerPrivate::linkletAcceptedConnection, this, _1));
    linklet->listen(address);
    m_listeners.push_back(linklet);
  } catch (LinkletListenFailed &e) {
    // Failed to listen on specified address
    return false;
  }

  return true;
}

void LinkManagerPrivate::close()
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

Contact LinkManagerPrivate::getLinkContact(const NodeIdentifier &linkId)
{
  RecursiveUniqueLock lock(m_linksMutex);
  if (m_links.find(linkId) == m_links.end())
    return Contact();

  return m_links[linkId]->contact();
}

void LinkManagerPrivate::removeLink(LinkPtr link)
{
  RecursiveUniqueLock lock(m_linksMutex);
  if (m_links.find(link->nodeId()) == m_links.end() ||
      m_links[link->nodeId()] != link)
    return;

  link->signalMessageReceived.disconnect(boost::bind(&LinkManagerPrivate::linkMessageReceived, this, _1));
  m_links.erase(link->nodeId());
}

std::list<NodeIdentifier> LinkManagerPrivate::getLinkIds()
{
  RecursiveUniqueLock lock(m_linksMutex);
  std::list<NodeIdentifier> links;
  for (auto p : m_links) {
    links.push_back(p.first);
  }
  return links;
}

void LinkManagerPrivate::linkMessageReceived(const Message &msg)
{
  m_statistics.global.msgRcvd++;
  m_statistics.links[msg.originator()].msgRcvd++;

  try {
    q.signalMessageReceived(msg);
  } catch (MessageCastFailed &e) {
    BOOST_LOG_SEV(m_logger, log::warning) << "Message parsing has failed on incoming message!";
  }
}

bool LinkManagerPrivate::verifyPeer(const Contact &contact)
{
  // If peer has the same identifier as the local node, we should drop the link
  if (contact.nodeId() == m_privateKey.nodeId()) {
    BOOST_LOG_SEV(m_logger, log::warning) << "Attempted nodeId collision, refusing link.";
    return false;
  }

  // Invoke externally registered verification hooks
  return q.signalVerifyPeer(contact);
}

void LinkManagerPrivate::linkletAcceptedConnection(LinkletPtr linklet)
{
  // Create and register a new link from the given linklet
  LinkPtr link = getOrCreateLink(linklet->peerContact());
  if (!link) {
    linklet->close();
    return;
  }

  try {
    link->addLinklet(linklet);
  } catch (TooManyLinklets &e) {
    link->tryCleanup();
    linklet->close();
  }
}

Contact LinkManagerPrivate::getLocalContact() const
{
  // TODO This should probably be cached
  Contact contact(m_privateKey.publicKey());
  for (const LinkletPtr &linklet : m_listeners) {
    Address address = linklet->address();
    if (address.type() == Address::Type::IP) {
      auto endpoint = address.toIpEndpoint();
      auto ip = endpoint.address();

      if ((ip.is_v4() && ip.to_v4() == boost::asio::ip::address_v4::any()) ||
        (ip.is_v6() && ip.to_v6() == boost::asio::ip::address_v6::any())) {
        // Listen on any interface, we need the introspector to discover all
        // available addresses
        Contact lcontact = HostIntrospector::localContact(endpoint.port());
        for (const auto &laddr : lcontact.addresses()) {
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

LinkManager::LinkManager(Context &context, const PrivatePeerKey &privateKey)
  : d(new LinkManagerPrivate(context, privateKey, *this))
{
}

Context &LinkManager::context() const
{
  return d->m_context;
}

void LinkManager::send(const Contact &contact, const Message &msg)
{
  d->send(contact, msg);
}

void LinkManager::send(const NodeIdentifier &nodeId, const Message &msg)
{
  d->send(nodeId, msg);
}

bool LinkManager::listen(const Address &address)
{
  d->listen(address);
}

void LinkManager::close()
{
  d->close();
}

Contact LinkManager::getLinkContact(const NodeIdentifier &linkId)
{
  return d->getLinkContact(linkId);
}

std::list<NodeIdentifier> LinkManager::getLinkIds()
{
  return d->getLinkIds();
}

const LinkletFactory &LinkManager::getLinkletFactory() const
{
  return d->m_linkletFactory;
}

Contact LinkManager::getLocalContact() const
{
  return d->getLocalContact();
}

const NodeIdentifier &LinkManager::getLocalNodeId() const
{
  return d->m_privateKey.nodeId();
}

const PrivatePeerKey &LinkManager::getLocalPrivateKey() const
{
  return d->m_privateKey;
}

void LinkManager::setLocalAddress(const Address &address)
{
  d->m_localAddress = Address(address.toIpEndpoint().address(), 0);
}

const Address &LinkManager::getLocalAddress() const
{
  return d->m_localAddress;
}

bool LinkManager::verifyPeer(const Contact &contact)
{
  return d->verifyPeer(contact);
}

const LinkManager::Statistics &LinkManager::statistics() const
{
  return d->m_statistics;
}

void LinkManager::removeLink(LinkPtr link)
{
  d->removeLink(link);
}

}
