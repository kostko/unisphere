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
#include "interplex/curvecp_linklet.h"
#include "interplex/exceptions.h"
#include "src/interplex/interplex.pb.h"

namespace UniSphere {

CurveCPLinklet::CurveCPLinklet(LinkManager &manager)
  : Linklet(manager),
    m_acceptor(m_service),
    m_stream(m_service)
{
}

CurveCPLinklet::~CurveCPLinklet()
{
}

void CurveCPLinklet::listen(const Address &address)
{
  curvecp::stream::endpoint endpoint(address.toUdpIpEndpoint());
  try {
    const PrivateBoxKey &boxKey = m_manager.getLocalPrivateKey().privateBoxSubkey();
    m_acceptor.set_local_extension(std::string("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16));
    m_acceptor.set_local_public_key(boxKey.raw());
    m_acceptor.set_local_private_key(boxKey.privateRaw());
    m_acceptor.set_nonce_generator(boost::bind(&Botan::AutoSeeded_RNG::randomize, &m_rng, _1, _2));
    m_acceptor.bind(endpoint);
    m_state = State::Listening;
    m_connectAddress = Address(m_acceptor.local_endpoint());
  } catch (boost::exception &e) {
    throw LinkletListenFailed();
  }

  // Log our listen attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Listening for incoming connections.";

  // Start accepting connections
  CurveCPLinkletPtr linklet(boost::make_shared<CurveCPLinklet>(m_manager));
  linklet->signalConnectionSuccess.connect(signalAcceptedConnection);
  m_acceptor.async_accept(
    linklet->m_stream,
    m_strand.wrap(boost::bind(&CurveCPLinklet::handleAccept, this, linklet, _1))
  );

  m_acceptor.listen();
}

void CurveCPLinklet::connect(const PublicPeerKey &peerKey, const Address &address)
{
  m_connectAddress = address;
  m_state = State::Connecting;

  // Log our connection attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Connecting to " << address.toUdpIpEndpoint()
    << " (id " << peerKey.nodeId().hex() << ").";

  // When a local address is specified, we should bind to it for outgoing connections
  Address localAddress = m_manager.getLocalAddress();
  if (!localAddress.isNull()) {
    m_stream.bind(localAddress.toUdpIpEndpoint());
  }

  const PrivateBoxKey &boxKey = m_manager.getLocalPrivateKey().privateBoxSubkey();
  m_stream.set_local_extension(std::string("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16));
  m_stream.set_local_public_key(boxKey.raw());
  m_stream.set_local_private_key(boxKey.privateRaw());
  m_stream.set_remote_extension(std::string("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16));
  m_stream.set_remote_public_key(peerKey.boxSubkey().raw());
  m_stream.set_remote_domain_name("uni.sphere");
  m_stream.set_nonce_generator(boost::bind(&Botan::AutoSeeded_RNG::randomize, &m_rng, _1, _2));

  // Setup the CurveCP socket
  m_stream.async_connect(
    address.toUdpIpEndpoint(),
    m_strand.wrap(boost::bind(&CurveCPLinklet::handleConnect,
      boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
      boost::asio::placeholders::error))
  );
}

void CurveCPLinklet::close()
{
  if (m_state == State::Closed)
    return;

  // Dispatch actual close event via our strand to avoid multiple simultaneous closes
  CurveCPLinkletPtr self = boost::static_pointer_cast<CurveCPLinklet>(shared_from_this());
  m_strand.dispatch([this, self]() {
    if (m_state == State::Closed)
      return;

    BOOST_LOG_SEV(m_logger, log::normal) << "Closing connection with " << m_peerContact.nodeId().hex() << ".";
    State state = m_state;
    m_state = State::Closed;
    m_stream.async_close([self]() {});

    // Emit the proper signal accoording to previous connection state
    if (state == State::Connected) {
      signalDisconnected(shared_from_this());
    } else {
      signalConnectionFailed(shared_from_this());
    }
  });
}

void CurveCPLinklet::send(const Message &msg)
{
  if (m_state != State::Connected && msg.type() != Message::Type::Interplex_Hello)
    return;

  UniqueLock lock(m_outMessagesMutex);
  bool sendInProgress = !m_outMessages.empty();
  m_outMessages.push_back(msg);

  if (!sendInProgress) {
    boost::asio::async_write(
      m_stream,
      boost::asio::buffer(m_outMessages.front().buffer()),
      m_strand.wrap(boost::bind(&CurveCPLinklet::handleWrite,
        boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
        boost::asio::placeholders::error))
    );
  }
}

void CurveCPLinklet::handleAccept(CurveCPLinkletPtr linklet, const boost::system::error_code &error)
{
  BOOST_LOG_SEV(m_logger, log::normal) << "Accepted new connection.";

  if (error) {
    return;
  }

  linklet->start();

  // Accept next connection
  linklet = boost::make_shared<CurveCPLinklet>(m_manager);
  linklet->signalConnectionSuccess.connect(signalAcceptedConnection);
  m_acceptor.async_accept(
    linklet->m_stream,
    m_strand.wrap(boost::bind(&CurveCPLinklet::handleAccept, this, linklet, _1))
  );
}

void CurveCPLinklet::handleConnect(const boost::system::error_code &error)
{
  if (!error) {
    // Connection successful
    BOOST_LOG_SEV(m_logger, log::normal) << "Outgoing connection successful.";
    start();
  } else {
    // Signal connection failure to upper layers
    BOOST_LOG_SEV(m_logger, log::warning) << "Outgoing connection failed!";
    signalConnectionFailed(shared_from_this());
  }
}

void CurveCPLinklet::start()
{
  BOOST_ASSERT(m_state != State::Listening);
  m_state = State::IntroWait;

  // Send introductory message
  Protocol::Interplex::Hello hello;
  *hello.mutable_local_contact() = m_manager.getLocalContact().toMessage();
  send(Message(Message::Type::Interplex_Hello, hello));

  // Wait for the introductory message
  boost::asio::async_read(m_stream,
    boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
    m_strand.wrap(boost::bind(&CurveCPLinklet::handleReadHeader,
      boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
  );
}

void CurveCPLinklet::handleReadHeader(const boost::system::error_code &error, size_t bytes)
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
      boost::asio::async_read(m_stream,
        boost::asio::buffer(&m_inMessage.buffer()[0] + Message::header_size, payloadSize),
        m_strand.wrap(boost::bind(&CurveCPLinklet::handleReadPayload,
          boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message header read failed!";
    close();
  }
}

void CurveCPLinklet::handleReadPayload(const boost::system::error_code &error)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;

  if (!error) {
    if (!Linklet::messageParsed(m_inMessage))
      return close();

    // Ready for next message
    boost::asio::async_read(m_stream,
      boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
      m_strand.wrap(boost::bind(&CurveCPLinklet::handleReadHeader,
        boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
    );
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message body read failed!";
    close();
  }
}

void CurveCPLinklet::handleWrite(const boost::system::error_code &error)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;

  if (!error) {
    UniqueLock lock(m_outMessagesMutex);
    m_outMessages.pop_front();
    if (!m_outMessages.empty()) {
      boost::asio::async_write(
        m_stream,
        boost::asio::buffer(m_outMessages.front().buffer()),
        m_strand.wrap(boost::bind(&CurveCPLinklet::handleWrite,
          boost::static_pointer_cast<CurveCPLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message write failed!";
    close();
  }
}

}
