/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_INTERPLEX_LINK_H
#define UNISPHERE_INTERPLEX_LINK_H

#include "core/globals.h"
#include "core/context.h"

#include "interplex/contact.h"
#include "interplex/message.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/foreach.hpp>

#include <deque>
#include <set>

namespace UniSphere {

class LinkManager;
UNISPHERE_SHARED_POINTER(Linklet)
UNISPHERE_SHARED_POINTER(MessageDispatcher)

/**
 * Represents a connection between two UNISPHERE nodes. It supports a
 * message-based protocol for communication between the nodes with
 * automatic link management and message queuing.
 */
class UNISPHERE_NO_EXPORT Link : public boost::enable_shared_from_this<Link> {
public:
  friend class LinkManager;
  friend class Linklet;
  
  /**
   * Possible link states.
   */
  enum class State {
    Invalid,
    Closed,
    Connecting,
    Connected
  };
  
  /**
   * Class destructor.
   */
  ~Link();
  
  /**
   * Returns true if this link is currently in a connected state.
   */
  inline bool isConnected() const { return m_state == State::Connected; }
  
  /**
   * Returns true if this link is currently in the invalid state.
   */
  inline bool isValid() const { return m_state != State::Invalid; }
  
  /**
   * Returns the node identifier of the node on the other side of
   * this link.
   */
  NodeIdentifier nodeId() const { return m_nodeId; } 
  
  /**
   * Returns the current link state.
   */
  State state() const { return m_state; }
  
  /**
   * Returns contact information about the destination node.
   */
  Contact contact();
protected:
  /// Signal the recipt of a message
  boost::signals2::signal<void (const Message&)> signalMessageReceived;
protected:
  /**
   * Private constructor.
   *
   * @param manager Link manager instance
   * @param nodeId Identifier of the node on the other end
   * @param maxIdleTime Maximum number of seconds a link can be idle
   */
  Link(LinkManager &manager, const NodeIdentifier &nodeId, time_t maxIdleTime);
  
  /**
   * Performs any post-construct initialization. This is required because
   * shared_from_this() is not available in the constructor.
   */
  void init();
  
  /**
   * Sends a UNISPHERE message through this link.
   *
   * @param msg Message to send
   */
  void send(const Message &msg);
  
  /**
   * Closes this link. After calling this method, the link should be
   * considered invalid and should not be accessed! When there is a 
   * possibility of calling this method from inside the Link class with
   * internal mutex held, you must take ownership before acquiring the
   * mutex or it will abort!
   */
  void close();
  
  /**
   * Adds a new linklet to this link. This may cause the link status
   * to change. The linklet MUST be connected to the same node this
   * link represents.
   *
   * @param linklet Linklet to add
   * @throws TooManyLinklets
   */
  void addLinklet(LinkletPtr linklet);
  
  /**
   * Removes the given linklet from this link.
   *
   * @param linklet Linklet to remove
   */
  void removeLinklet(LinkletPtr linklet);
  
  /**
   * Checks linklets and ensures that link state is in sync with
   * all the linklets.
   */
  void checkLinkletState();
  
  /**
   * Adds new contact information to this link.
   * 
   * @param contact Contact information
   */
  void addContact(const Contact &contact);
  
  /**
   * Attempts to connect to the next address in the address list.
   */
  void tryNextAddress();
  
  /**
   * Checks if this link should be closed due to no linklets being
   * available.
   */
  void tryCleanup();
  
  /**
   * Changes the link's state.
   * 
   * @param state New state
   */
  void setState(State state);
protected:
  /**
   * Called by @ref Linklet when current connection attempt is
   * unsuccessful.
   *
   * @param linklet Affected linklet
   */
  void linkletConnectionFailed(LinkletPtr linklet);
  
  /**
   * Called by @ref Linklet when additional peer verification should
   * be performed. It should return true if the verification passes.
   *
   * @param linklet Affected linklet
   */
  bool linkletVerifyPeer(LinkletPtr linklet);
  
  /**
   * Called by @ref Linklet when current connection attempt is
   * successful.
   *
   * @param linklet Affected linklet
   */
  void linkletConnectionSuccess(LinkletPtr linklet);
  
  /**
   * Called by @ref Linklet when an active connection gets closed
   * due to network problems or timeouts.
   *
   * @param linklet Affected linklet
   */
  void linkletDisconnected(LinkletPtr linklet);
  
  /**
   * Called by @ref Linklet when a message has been received.
   *
   * @param linklet Publishing linklet
   * @param message Received message
   */
  void linkletMessageReceived(LinkletPtr linklet, const Message &message);
  
  /**
   * Called when the retry timer expires.
   * 
   * @param error Potential error code
   */
  void retryTimeout(const boost::system::error_code &error);
  
  /**
   * Called when the idle timer expires.
   * 
   * @param error Potential error code
   */
  void idleTimeout(const boost::system::error_code &error);
private:
  /// Manager responsible for this link
  LinkManager &m_manager;
  /// Logger instance
  Logger m_logger;
  /// Other end of this link
  NodeIdentifier m_nodeId;
  /// Current link state
  State m_state;
  /// Maximum number of seconds a link can be idle without being closed
  time_t m_maxIdleTime;
  /// Internal mutex
  std::recursive_mutex m_mutex;
  
  /// Messages queued for delivery when the link was down
  std::deque<Message> m_messages;
  /// Message dispatcher instance
  MessageDispatcherPtr m_dispatcher;
  
  /// List of linklets
  std::list<LinkletPtr> m_linklets;
  
  /// Contact address list
  std::set<Address> m_addressList;
  /// Contact address list iterator
  std::set<Address>::const_iterator m_addressIterator;
  
  /// Timer for retrying connection attempts
  boost::asio::deadline_timer m_retryTimer;
  /// Timer for timing out idle links
  boost::asio::deadline_timer m_idleTimer;
};

UNISPHERE_SHARED_POINTER(Link)

}

#endif
