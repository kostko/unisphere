/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "interplex/local_linklet.h"
#include "interplex/link_manager.h"
#include "interplex/exceptions.h"
#include "src/interplex/interplex.pb.h"

#include <boost/bind.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/filesystem.hpp>

namespace UniSphere {

LocalLinklet::LocalLinklet(LinkManager &manager)
  : Linklet(manager),
    m_logger(logging::keywords::channel = "local_linklet"),
    m_acceptor(m_service),
    m_socket(m_service)
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(manager.getLocalNodeId()));
}

LocalLinklet::~LocalLinklet()
{
}

boost::asio::local::stream_protocol::socket &LocalLinklet::socket()
{
  return m_socket;
}

void LocalLinklet::listen(const Address &address)
{
  boost::asio::local::stream_protocol::endpoint endpoint(address.toLocalEndpoint());
  try {
    // Try to remove a previous socket allocation before creating a new one
    boost::filesystem::remove(endpoint.path());
  } catch (boost::filesystem::filesystem_error &e) {
  }

  try {
    m_acceptor.open(endpoint.protocol());
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
    m_state = State::Listening;
    m_connectAddress = Address(m_acceptor.local_endpoint());
  } catch (boost::exception &e) {
    throw LinkletListenFailed();
  }
  
  // Log our listen attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Listening for incoming connections.";
  
  // Setup the local acceptor
  LinkletPtr linklet(new LocalLinklet(m_manager));
  linklet->signalConnectionSuccess.connect(signalAcceptedConnection);
  m_acceptor.async_accept(
    boost::static_pointer_cast<LocalLinklet>(linklet)->socket(),
    m_strand.wrap(boost::bind(&LocalLinklet::handleAccept, this, linklet, boost::asio::placeholders::error))
  );
}

void LocalLinklet::connect(const Address &address)
{
  m_connectAddress = address;
  m_state = State::Connecting;
  
  // Log our connection attempt
  BOOST_LOG_SEV(m_logger, log::normal) << "Connecting to a remote address...";
  
  // Setup the local socket
  socket().async_connect(
    address.toLocalEndpoint(),
    m_strand.wrap(boost::bind(&LocalLinklet::handleConnect, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
      boost::asio::placeholders::error))
  );
}

void LocalLinklet::close()
{
  if (m_state == State::Closed)
    return;
  
  // Dispatch actual close event via our strand to avoid multiple simultaneous closes
  LocalLinkletPtr self = boost::static_pointer_cast<LocalLinklet>(shared_from_this());
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

void LocalLinklet::start(bool client)
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
    m_strand.wrap(boost::bind(&LocalLinklet::handleReadHeader, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
  );
}

void LocalLinklet::handleAccept(LinkletPtr linklet, const boost::system::error_code &error)
{
  if (!error) {
    // Start the connection
    boost::static_pointer_cast<LocalLinklet>(linklet)->start(false);
    
    // Ready for the next connection
    LinkletPtr nextLinklet(new LocalLinklet(m_manager));
    nextLinklet->signalConnectionSuccess.connect(signalAcceptedConnection);
    m_acceptor.async_accept(
      boost::static_pointer_cast<LocalLinklet>(nextLinklet)->socket(),
      m_strand.wrap(boost::bind(&LocalLinklet::handleAccept, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
        nextLinklet, boost::asio::placeholders::error))
    );
  } else {
    // TODO Error while accepting, how to handle this?
  }
}

void LocalLinklet::handleConnect(const boost::system::error_code &error)
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

void LocalLinklet::send(const Message &msg)
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
      m_strand.wrap(boost::bind(&LocalLinklet::handleWrite, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
        boost::asio::placeholders::error))
    );
  }
}

void LocalLinklet::handleWrite(const boost::system::error_code &error)
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
        m_strand.wrap(boost::bind(&LocalLinklet::handleWrite, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message write failed!";
    close();
  }
}

void LocalLinklet::handleReadHeader(const boost::system::error_code &error, size_t bytes)
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
      if (m_state == State::IntroWait) {
        // Only hello messages are allowed in IntroWait state
        if (m_inMessage.type() != Message::Type::Interplex_Hello) {
          BOOST_LOG_SEV(m_logger, log::error) << "Received non-hello message in IntroWait phase!";
          close();
          return;
        }
      }
      
      // Read payload
      boost::asio::async_read(m_socket,
        boost::asio::buffer(&m_inMessage.buffer()[0] + Message::header_size, payloadSize),
        m_strand.wrap(boost::bind(&LocalLinklet::handleReadPayload, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
          boost::asio::placeholders::error))
      );
    }
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message header read failed!";
    close();
  }
}

void LocalLinklet::handleReadPayload(const boost::system::error_code &error)
{
  // Ignore aborted ASIO operations
  if (error == boost::asio::error::operation_aborted || m_state == State::Closed)
    return;
  
  if (!error) {
    if (m_state == State::IntroWait) {
      // We have received the hello message, extract information and detach
      Protocol::Interplex::Hello hello = message_cast<Protocol::Interplex::Hello>(m_inMessage);
      Contact peerContact = Contact::fromMessage(hello.local_contact());
      if (peerContact.isNull()) {
        BOOST_LOG_SEV(m_logger, log::error) << "Invalid peer contact in hello message!";
        return close();
      }
      
      m_peerContact = peerContact;
      
      // Perform additional verification on the peer before transitioning into
      // the connected state
      if (!signalVerifyPeer(shared_from_this()) || !m_manager.verifyPeer(peerContact)) {
        return close();
      }
      
      BOOST_LOG_SEV(m_logger, log::normal) << "Introductory phase with " << peerContact.nodeId().hex() << " completed.";
      m_state = State::Connected;
      signalConnectionSuccess(shared_from_this());
      
      // The above signal may close this linklet, in this case we should not start reading,
      // otherwise this will cause a segmentation fault
      if (m_state != State::Connected)
        return;
    } else {
      // Payload has been read, emit message and detach
      signalMessageReceived(shared_from_this(), m_inMessage);
    }
    
    m_inMessage.detach();
    
    // Ready for next message
    boost::asio::async_read(m_socket,
      boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
      m_strand.wrap(boost::bind(&LocalLinklet::handleReadHeader, boost::static_pointer_cast<LocalLinklet>(shared_from_this()),
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))
    );
  } else {
    // Log
    BOOST_LOG_SEV(m_logger, log::warning) << "Message body read failed!";
    close();
  }
}

}
