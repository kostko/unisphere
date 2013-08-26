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
#include "interplex/message.h"

#ifdef UNISPHERE_DEBUG
#include "measure/measure.h"
#endif

#include <unordered_map>
#include <boost/signals2/signal.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Link)

/**
 * The verify peer combiner is used to call slots bound to the verifyPeer
 * signal. If any slot returns false, further slots are not called and
 * false is returned as a final result, meaning that the connection will
 * be aborted.
 */
class UNISPHERE_EXPORT VerifyPeerCombiner {
public:
  typedef bool result_type;
  
  template<typename InputIterator>
  result_type operator()(InputIterator first, InputIterator last)
  {
    if (first == last)
      return true;
    
    while (first != last) {
      if (*first == false)
        return false;
      ++first;
    }
    
    return true;
  }
};

/**
 * A link manager is used to manage links to all peers in a unified way.
 */
class UNISPHERE_EXPORT LinkManager {
public:
  friend class Link;

  /**
   * A structure for reporting link manager statistics.
   */
  struct Statistics {
    struct LinkStatistics {
      /// Number of transmitted messages
      size_t msgXmits = 0;
      /// Number of received messages
      size_t msgRcvd = 0;
    };

    /// Global statistics
    LinkStatistics global;
    /// Per-link statistics
    std::unordered_map<NodeIdentifier, LinkStatistics> links;
  };
  
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
   * Sends a message to the given contact address.
   * 
   * @param contact Destination node's contact information
   * @param msg Message to send
   */
  void send(const Contact &contact, const Message &msg);
  
  /**
   * Sends a message to the given peer. If there is no existing link
   * for the specified peer, message will not be delivered.
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
  
#ifdef UNISPHERE_DEBUG
  /**
   * Returns the measure instance that can be used for storing measurements.
   */
  inline Measure &getMeasure() { return m_measure; }
#endif

  /**
   * Invokes registered peer verification hooks (signalVerifyPeer) and
   * returns the result.
   *
   * @param contact Contact to verify
   * @return True if verification was successful, false otherwise
   */
  bool verifyPeer(const Contact &contact);

  /**
   * Retrieves various statistics about link manager operation.
   */
  const Statistics &statistics() const;
public:
  /// Signal for received messages
  boost::signals2::signal<void (const Message&)> signalMessageReceived;
  /// Signal for additional peer verification
  boost::signals2::signal<bool (const Contact&), VerifyPeerCombiner> signalVerifyPeer;
protected:
  /**
   * Returns a link suitable for communication with the specified contact.
   * 
   * @param contact Contact information
   * @param create Should a new link be created if none is found
   * @return A valid link instance or null
   */
  LinkPtr get(const Contact &contact, bool create = true);
  
  /**
   * Removes a specific link.
   *
   * @param link Link instance
   */
  void remove(LinkPtr link);
protected:
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
private:
  // TODO: Make this private via PIMPL

  /// UNISPHERE context this manager belongs to
  Context& m_context;
  /// Logger instance
  Logger m_logger;
  
  /// Local node identifier
  NodeIdentifier m_nodeId;
  
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

#ifdef UNISPHERE_DEBUG
  /// Measurement instance
  Measure m_measure;
#endif

  /// Statistics
  Statistics m_statistics;
};
  
}

// Logging and measurement macros
#ifdef UNISPHERE_DEBUG
#define UNISPHERE_MEASURE_ADD(manager, metric, value) (manager).getMeasure().add(metric, value)
#define UNISPHERE_MEASURE_INC(manager, metric) (manager).getMeasure().increment(metric)
#define UNISPHERE_MEASURE_SET(manager, metric, value) (manager).getMeasure().set(metric, value)
#else
#define UNISPHERE_MEASURE_ADD(manager, metric, value)
#define UNISPHERE_MEASURE_INC(manager, metric)
#define UNISPHERE_MEASURE_SET(manager, metric, value)
#endif

#endif
