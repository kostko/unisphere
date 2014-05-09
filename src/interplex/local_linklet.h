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
#ifndef UNISPHERE_INTERPLEX_LOCALLINKLET_H
#define UNISPHERE_INTERPLEX_LOCALLINKLET_H

#include "core/globals.h"
#include "interplex/linklet.h"

#include <deque>
#include <boost/asio.hpp>

namespace UniSphere {

class UNISPHERE_NO_EXPORT LocalLinklet : public Linklet {
public:
  /**
   * Class constructor.
   *
   * @param manager Link manager instance
   */
  LocalLinklet(LinkManager &manager);

  LocalLinklet(const LocalLinklet&) = delete;
  LocalLinklet &operator=(const LocalLinklet&) = delete;

  /**
   * Class destructor.
   */
  virtual ~LocalLinklet();

  /**
   * Starts listening for incoming connections on the given address.
   *
   * @param address Address to listen on
   */
  void listen(const Address &address) override;

  /**
   * Starts connecting to the given address.
   *
   * @param peerKey Public peer key
   * @param address Address to connect to
   */
  void connect(const PublicPeerKey &peerKey, const Address &address) override;

  /**
   * Closes the link.
   */
  void close() override;

  /**
   * Sends a message via this link.
   *
   * @param msg The message to send
   */
  void send(const Message &msg) override;

  /**
   * Starts the handshake.
   *
   * @param client Perform client handshake
   */
  void start(bool client = true);

  /**
   * Returns the ASIO local socket used by this linklet.
   */
  boost::asio::local::stream_protocol::socket &socket();
protected:
  /**
   * Handles accept of incoming local connections.
   *
   * @param linklet Linklet to use for this new connection
   * @param error Potential error code
   */
  void handleAccept(LinkletPtr linklet, const boost::system::error_code &error);

  /**
   * Handles completion of a connect operation.
   *
   * @param error Potential error code
   */
  void handleConnect(const boost::system::error_code &error);

  /**
   * Handles reading of header via socket.
   *
   * @param error Potential error code
   * @param bytes Number of bytes read
   */
  void handleReadHeader(const boost::system::error_code &error, size_t bytes);

  /**
   * Handles reading of payload via socket.
   *
   * @param error Potential error code
   */
  void handleReadPayload(const boost::system::error_code &error);

  /**
   * Handles packet submission.
   *
   * @param error Potential error code
   */
  void handleWrite(const boost::system::error_code &error);
protected:
  // ASIO local acceptor (when handling incoming connections)
  boost::asio::local::stream_protocol::acceptor m_acceptor;
  // ASIO local socket (for actual data transfer)
  boost::asio::local::stream_protocol::socket m_socket;
  // Incoming message and outgoing message queue
  Message m_inMessage;
  std::deque<Message> m_outMessages;
  std::mutex m_outMessagesMutex;
};

UNISPHERE_SHARED_POINTER(LocalLinklet)

}

#endif
