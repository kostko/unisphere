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
#ifndef UNISPHERE_INTERPLEX_LINKLET_H
#define UNISPHERE_INTERPLEX_LINKLET_H

#include "core/globals.h"
#include "interplex/contact.h"
#include "interplex/message.h"
#include "interplex/link_manager.h"

#include <boost/asio.hpp>
#include <boost/signal.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Linklet)

/**
 * The verify peer combiner is used to call slots bound to the verifyPeer
 * signal. If any slot returns false, further slots are not called and
 * false is returned as a final result, meaning that the connection will
 * be aborted.
 */
class UNISPHERE_NO_EXPORT VerifyPeerCombiner {
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
 * Linklets represent different transport protocols that can be used
 * by Interplex to interconnect nodes.
 */
class UNISPHERE_NO_EXPORT Linklet : public boost::enable_shared_from_this<Linklet> {
public:
  /**
   * Possible linklet states.
   */
  enum class State {
    Closed,
    Connecting,
    IntroWait,
    Connected,
    Listening
  };
  
  /**
   * Class constructor.
   *
   * @param manager Link manager instance
   */
  Linklet(LinkManager &manager);
  
  /**
   * Class destructor.
   */
  virtual ~Linklet();
  
  /**
   * Returns the address this linklet is connected to. For listener linklets
   * this will be the bound address.
   */
  inline Address address() const { return m_connectAddress; }
  
  /**
   * Returns the peer contact information.
   */
  inline Contact peerContact() const { return m_peerContact; }
  
  /**
   * Returns the linklet's connection state.
   */
  inline State state() const { return m_state; }
  
  /**
   * Starts listening for incoming connections on the given address.
   *
   * @param address Address to listen on
   */
  virtual void listen(const Address &address) = 0;
  
  /**
   * Starts connecting to the given address.
   *
   * @param address Address to connect to
   */
  virtual void connect(const Address &address) = 0;
  
  /**
   * Closes the link.
   */
  virtual void close() = 0;
  
  /**
   * Sends a message via this link.
   *
   * @param msg The message to send
   */
  virtual void send(const Message &msg) = 0;
public:
  // Signals
  boost::signal<void (LinkletPtr)> signalConnectionFailed;
  boost::signal<void (LinkletPtr)> signalConnectionSuccess;
  boost::signal<bool (LinkletPtr), VerifyPeerCombiner> signalVerifyPeer;
  boost::signal<void (LinkletPtr)> signalDisconnected;
  boost::signal<void (LinkletPtr)> signalAcceptedConnection;
  boost::signal<void (LinkletPtr, const Message&)> signalMessageReceived;
protected:
  /// Link manager this linklet belongs to
  LinkManager &m_manager;
  
  /// ASIO service
  boost::asio::io_service &m_service;
  /// ASIO strand
  boost::asio::strand m_strand;
  
  /// Address this linklet is connected to
  Address m_connectAddress;
  /// Peer contact information received as part of the exchange
  Contact m_peerContact;
  
  /// Linklet state
  State m_state;
}; 

}

#endif
