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

#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Link)
  
class UNISPHERE_EXPORT LinkManager {
public:
  LinkManager(Context &context, const NodeIdentifier &nodeId);
  
  /**
   * Returns the UNISPHERE context this manager belongs to.
   */
  inline Context &context() const { return m_context; }
  
  LinkPtr connect(const Contact &contact, std::function<void(Link&)> init = NULL);
  
  LinkPtr create(const Contact &contact, std::function<void(Link&)> init, bool connect);
  
  void listen(const Contact &contact);
  
  void listen(const Address &address, const NodeIdentifier &nodeId);
  
  /**
   * Removes the link to a specific node. The link must be in the Closed
   * state.
   *
   * @param nodeId Link node identifier
   */
  void remove(const NodeIdentifier &nodeId);
  
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
   * Sets up the initialization function for incoming links.
   *
   * @param init The initialization function
   */
  void setListenLinkInit(std::function<void(Link&)> init);
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
  boost::shared_mutex m_linksMutex;
  
  /// A list of all listening linklets
  std::list<LinkletPtr> m_listeners;
  /// Mutex protecting the listening linklet list
  boost::shared_mutex m_listenersMutex;
  /// Initialization function for listening links
  std::function<void(Link&)> m_listeningInit;
};
  
}

#endif
