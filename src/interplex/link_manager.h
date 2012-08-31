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
#ifndef UNISPHERE_INTERPLEX_LINKMANAGER_H
#define UNISPHERE_INTERPLEX_LINKMANAGER_H

#include "core/context.h"
#include "interplex/contact.h"
#include "interplex/linklet_factory.h"

#ifdef UNISPHERE_DEBUG
#include "measure/measure.h"
#endif

#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Link)

/**
 * A link manager is used to manage links to all peers in a unified way. Each
 * link is exposed as a Link instance with a simple message-based interface.
 * The underlying mechanism of actually delivering these messages is completely
 * abstracted.
 */
class UNISPHERE_EXPORT LinkManager {
public:
  /**
   * Constructs a new link manager instance.
   * 
   * @param context UNISPHERE context
   * @param nodeId Node identifier of the local node
   */
  LinkManager(Context &context, const NodeIdentifier &nodeId);
  
  LinkManager(const LinkManager&) = delete;
  LinkManager &operator=(const LinkManager&) = delete;
  
  /**
   * Returns the UNISPHERE context this manager belongs to.
   */
  inline Context &context() const { return m_context; }
  
  /**
   * Creates a new link and starts with the connection procedure.
   * 
   * @param contact Peer contact information
   * @return A shared instance of Link
   */
  LinkPtr connect(const Contact &contact);
  
  /**
   * Creates a new link if one doesn't yet exist for the specified
   * contact. If a link to this contact already exists, it is returned
   * instead.
   * 
   * @param contact Peer contact information
   * @param connect True to immediately start with the connection procedure
   * @return A shared instance of Link
   */
  LinkPtr create(const Contact &contact, bool connect);
  
  /**
   * Opens a listening linklet.
   * 
   * @param address Local address to listen on
   * @return True if listen was successful
   */
  bool listen(const Address &address);
  
  /**
   * Removes the link to a specific node. The link must be in the Closed
   * state.
   *
   * @param nodeId Link node identifier
   */
  void remove(const NodeIdentifier &nodeId);
  
  /**
   * Closes all existing links and stops listening for any new ones.
   */
  void closeAll();
  
  /**
   * Returns an existing link instance if one exists for the specified
   * node identifier. Otherwise NULL is returned.
   * 
   * @param nodeId Link node identifier
   * @return Link instance corresponding to the identifier or NULL
   */
  LinkPtr getLink(const NodeIdentifier &nodeId);
  
  /**
   * Returns a list of node identifiers to links that we have established.
   */
  std::list<NodeIdentifier> getLinkIds();
  
  /**
   * Returns the linklet factory instance.
   */
  inline const LinkletFactory &getLinkletFactory() const { return m_linkletFactory; }
  
  /**
   * Returns the local contact information.
   */
  Contact getLocalContact() const;
  
  /**
   * Returns the local node identifier.
   */
  inline const NodeIdentifier &getLocalNodeId() const { return m_nodeId; }
  
  /**
   * Sets a local address for all outgoing connections. This will cause all
   * outgoing sockets to bind to this address.
   * 
   * @param address Local outgoing address
   */
  void setLocalAddress(const Address &address);
  
  /**
   * Returns the local address.
   */
  inline const Address &getLocalAddress() const { return m_localAddress; }
  
  /**
   * Sets up a function that will be called each time a new link instance
   * needs to be initialized. This function should be used to setup any
   * signals or other link-related resources.
   *
   * @param init The initialization function
   */
  void setLinkInitializer(std::function<void(Link&)> init);
  
#ifdef UNISPHERE_DEBUG
  /**
   * Returns the measure instance that can be used for storing measurements.
   */
  inline Measure &getMeasure() { return m_measure; }
#endif
protected:
  /**
   * Called by a listener @ref Linklet when a new connection gets accepted
   * and is ready for dispatch.
   *
   * @param linklet New linklet for this connection
   */
  void linkletAcceptedConnection(LinkletPtr linklet);
private:
  /// UNISPHERE context this manager belongs to
  Context& m_context;
  
  /// Local node identifier
  NodeIdentifier m_nodeId;
  
  /// Linklet factory for producing new linklets
  LinkletFactory m_linkletFactory;
  
  /// Mapping of all managed links by their identifiers
  boost::unordered_map<NodeIdentifier, LinkPtr> m_links;
  /// Mutex protecting the link mapping
  std::recursive_mutex m_linksMutex;
  
  /// A list of all listening linklets
  std::list<LinkletPtr> m_listeners;
  /// Mutex protecting the listening linklet list
  std::mutex m_listenersMutex;
  
  /// Initialization function for new link instances
  std::function<void(Link&)> m_linkInitializer;
  
  /// Local outgoing address
  Address m_localAddress;

#ifdef UNISPHERE_DEBUG
  /// Measurement instance
  Measure m_measure;
#endif
};
  
}

// Logging and measurement macros
#ifdef UNISPHERE_DEBUG
#define UNISPHERE_LOG(manager, level, text) (manager).context().logger().output(Logger::Level::level, (text), (manager).getLocalNodeId().as(NodeIdentifier::Format::Hex))
#define UNISPHERE_MEASURE_ADD(manager, metric, value) (manager).getMeasure().add(metric, value)
#define UNISPHERE_MEASURE_INC(manager, metric) (manager).getMeasure().increment(metric)
#else
#define UNISPHERE_LOG(manager, level, text)
#define UNISPHERE_MEASURE_ADD(manager, metric, value)
#define UNISPHERE_MEASURE_INC(manager, metric)
#endif

#endif
