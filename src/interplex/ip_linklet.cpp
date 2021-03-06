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
#include "interplex/ip_linklet.h"
#include "interplex/link_manager.h"
#include "interplex/exceptions.h"
#include "src/interplex/interplex.pb.h"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

namespace UniSphere {

IPLinklet::IPLinklet(LinkManager &manager)
  : Linklet(manager),
    m_acceptor(m_service),
    m_socket(m_service)
{
}

IPLinklet::~IPLinklet()
{
}

boost::asio::ip::tcp::socket &IPLinklet::socket()
{
  return m_socket;
}

void IPLinklet::listen(const Address &address)
{
  boost::asio::ip::tcp::endpoint endpoint(address.toIpEndpoint());
  try {
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    m_acceptor.set_option(boost::asio::socket_base::keep_alive(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
    m_state = State::Listening;
    m_connectAddress = Address(m_acceptor.local_endpoint());
  } catch (boost::exception &e) {
    throw LinkletListenFailed();
  }

  // Log our listen attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Listening for incoming connections.";

  // Setup the TCP acceptor
  LinkletPtr linklet(boost::make_shared<IPLinklet>(m_manager));
  linklet->signalConnectionSuccess.connect(signalAcceptedConnection);
  m_acceptor.async_accept(
    boost::static_pointer_cast<IPLinklet>(linklet)->socket(),
    m_strand.wrap(boost::bind(&IPLinklet::handleAccept, this, linklet, boost::asio::placeholders::error))
  );
}

void IPLinklet::connect(const Address &address)
{
  m_connectAddress = address;
  m_state = State::Connecting;

  // Log our connection attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Connecting to " << address.toIpEndpoint() << " ...";

  // When a local address is specified, we should bind to it for outgoing connections
  Address localAddress = m_manager.getLocalAddress();
  if (!localAddress.isNull()) {
    socket().open(localAddress.toIpEndpoint().protocol());
    socket().bind(localAddress.toIpEndpoint());
  }

  // Setup the TCP socket
  socket().async_connect(
    address.toIpEndpoint(),
    m_strand.wrap(boost::bind(&IPLinklet::handleConnect, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
      boost::asio::placeholders::error))
  );
}

void IPLinklet::close()
{
  if (m_state == State::Closed)
    return;

  // Dispatch actual close event via our strand to avoid multiple simultaneous closes
  IPLinkletPtr self = boost::static_pointer_cast<IPLinklet>(shared_from_this());
  m_strand.dispatch([this, self]() {
    if (m_state == State::Closed)
      return;

    BOOST_LOG_SEV(m_logger, log::normal) << "Closing connection with " << m_peerContact.nodeId().hex() << ".";
    State state = m_state;
    m_state = State::Closed;
    socket().close();

    // Emit the proper signal accoording to previous connection state
    if (state == State::Connected) {
      signalDisconnected(shared_from_this());
    } else {
      signalConnectionFailed(shared_from_this());
    }
  });
}

void IPLinklet::start(bool client)
{
  BOOST_ASSERT(m_state != State::Listening);
  m_state = State::IntroWait;

  // Send introductory message
  Protocol::Interplex::Hello hello;
  *hello.mutable_local_contact() = m_manager.getLocalContact().toMessage();
  send(Message(Message::Type::Interplex_Hello, hello));

  // Wait for the introductory message
  boost::asio::async_read(m_socket,
    boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
    m_strand.wrap(boost::bind(&IPLinklet::handleReadHeader, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
  );
}

void IPLinklet::handleAccept(LinkletPtr linklet, const boost::system::error_code &error)
{
  if (!error) {
    // Start the connection
    boost::static_pointer_cast<IPLinklet>(linklet)->start(false);

    // Ready for the next connection
    LinkletPtr nextLinklet(boost::make_shared<IPLinklet>(m_manager));
    nextLinklet->signalConnectionSuccess.connect(signalAcceptedConnection);
    m_acceptor.async_accept(
      boost::static_pointer_cast<IPLinklet>(nextLinklet)->socket(),
      m_strand.wrap(boost::bind(&IPLinklet::handleAccept, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
        nextLinklet, boost::asio::placeholders::error))
    );
  } else {
    // TODO Error while accepting, how to handle this?
  }
}

void IPLinklet::handleConnect(const boost::system::error_code &error)
{
  if (!error) {
    // Connection successful
    BOOST_LOG_SEV(m_logger, log::normal) << "Outgoing connection successful.";
    start();
  } else {
    // Signal connection failure to upper layers
    BOOST_LOG_SEV(m_logger, log::warning) << "Outgoing connection failed: " << error.message();
    signalConnectionFailed(shared_from_this());
  }
}

void IPLinklet::send(const Message &msg)
{
  if (m_state != State::Connected && msg.type() != Message::Type::Interplex_Hello)
    return;

  UniqueLock lock(m_outMessagesMutex);
  bool sendInProgress = !m_outMessages.empty();
  m_outMessages.push_back(msg);

  if (!sendInProgress) {
    boost::asio::async_write(
      m_socket,
      boost::asio::buffer(m_outMessages.front().buffer()),
      m_strand.wrap(boost::bind(&IPLinklet::handleWrite, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
        boost::asio::placeholders::error))
    );
  }
}

void IPLinklet::handleWrite(const boost::system::error_code &error)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;

  if (!error) {
    UniqueLock lock(m_outMessagesMutex);
    m_outMessages.pop_front();
    if (!m_outMessages.empty()) {
      boost::asio::async_write(
        m_socket,
        boost::asio::buffer(m_outMessages.front().buffer()),
        m_strand.wrap(boost::bind(&IPLinklet::handleWrite, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message write failed!";
    close();
  }
}

void IPLinklet::handleReadHeader(const boost::system::error_code &error, size_t bytes)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;

  if (!error && bytes == Message::header_size) {
    // Parse header and get message size
    size_t payloadSize = m_inMessage.parseHeader();
    if (payloadSize == 0) {
      handleReadPayload(error);
    } else {
      if (!Linklet::headerParsed(m_inMessage))
        return close();

      // Read payload
      boost::asio::async_read(m_socket,
        boost::asio::buffer(&m_inMessage.buffer()[0] + Message::header_size, payloadSize),
        m_strand.wrap(boost::bind(&IPLinklet::handleReadPayload, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message header read failed!";
    close();
  }
}

void IPLinklet::handleReadPayload(const boost::system::error_code &error)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;

  if (!error) {
    if (!Linklet::messageParsed(m_inMessage))
      return close();

    // Ready for next message
    boost::asio::async_read(m_socket,
      boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
      m_strand.wrap(boost::bind(&IPLinklet::handleReadHeader, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
    );
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message body read failed!";
    close();
  }
}

}
