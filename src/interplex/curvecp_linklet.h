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
#ifndef UNISPHERE_INTERPLEX_CURVECPLINKLET_H
#define UNISPHERE_INTERPLEX_CURVECPLINKLET_H

#include "core/globals.h"
#include "interplex/linklet.h"

#include <deque>
#include <boost/asio.hpp>
#include <curvecp/curvecp.hpp>

namespace UniSphere {

class CurveCPLinklet;
UNISPHERE_SHARED_POINTER(CurveCPLinklet)

class UNISPHERE_NO_EXPORT CurveCPLinklet : public Linklet {
public:
  /**
   * Class constructor.
   *
   * @param manager Link manager instance
   */
  CurveCPLinklet(LinkManager &manager);

  CurveCPLinklet(const CurveCPLinklet&) = delete;
  CurveCPLinklet &operator=(const CurveCPLinklet&) = delete;

  /**
   * Class destructor.
   */
  virtual ~CurveCPLinklet();

  /**
   * Starts listening for incoming connections on the given address.
   *
   * @param address Address to listen on
   */
  void listen(const Address &address);

  /**
   * Starts connecting to the given address.
   *
   * @param peerKey Public peer key
   * @param address Address to connect to
   */
  void connect(const PublicPeerKey &peerKey, const Address &address);

  /**
   * Closes the link.
   */
  void close();

  /**
   * Sends a message via this link.
   *
   * @param msg The message to send
   */
  void send(const Message &msg);
protected:
  /**
   * Starts the introductory contact exchange.
   */
  void start();

  /**
   * Handles accept of incoming CurveCP connections.
   *
   * @param linklet Linklet to use for this new connection
   * @param error Potential error code
   */
  void handleAccept(CurveCPLinkletPtr linklet, const boost::system::error_code &error);

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
  /// CurveCP acceptor (when handling incoming connections)
  curvecp::acceptor m_acceptor;
  /// CurveCP stream
  curvecp::stream m_stream;
  /// Incoming message
  Message m_inMessage;
  /// Outgoing message queue
  std::deque<Message> m_outMessages;
  /// Mutex for outgoing messages
  std::mutex m_outMessagesMutex;
  /// Random nonce generator
  Botan::AutoSeeded_RNG m_rng;
};

}

#endif
