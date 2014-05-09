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
#ifndef UNISPHERE_INTERPLEX_LINKLET_H
#define UNISPHERE_INTERPLEX_LINKLET_H

#include "core/globals.h"
#include "core/signal.h"
#include "interplex/contact.h"
#include "interplex/message.h"
#include "interplex/link_manager.h"

#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Linklet)

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

  Linklet(const Linklet&) = delete;
  Linklet &operator=(const Linklet&) = delete;

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
   * @param peerKey Public peer key
   * @param address Address to connect to
   */
  virtual void connect(const PublicPeerKey &peerKey, const Address &address) = 0;

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
  boost::signals2::signal<void (LinkletPtr)> signalConnectionFailed;
  boost::signals2::signal<void (LinkletPtr)> signalConnectionSuccess;
  boost::signals2::signal<bool (LinkletPtr), AllTrueCombiner> signalVerifyPeer;
  boost::signals2::signal<void (LinkletPtr)> signalDisconnected;
  boost::signals2::signal<void (LinkletPtr)> signalAcceptedConnection;
  boost::signals2::signal<void (LinkletPtr, const Message&)> signalMessageReceived;
protected:
  /// Logger instance
  Logger m_logger;
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
protected:
  /**
   * This method must be called by linklet implementations when a message
   * header has been parsed.
   *
   * @param msg Message with header set
   * @return True if the linklet should be closed, false otherwise
   */
  bool headerParsed(Message &msg);

  /**
   * This method must be called by linklet implementations when a message
   * body has been parsed.
   *
   * @param msg A complete message
   * @return True if the linklet should be closed, false otherwise
   */
  bool messageParsed(Message &msg);
};

}

#endif
