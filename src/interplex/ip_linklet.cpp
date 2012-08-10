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
#include "interplex/ip_linklet.h"
#include "interplex/link_manager.h"
#include "interplex/exceptions.h"
#include "src/interplex/interplex.pb.h"

#include <boost/bind.hpp>

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
    m_connectAddress = address;
  } catch (boost::exception &e) {
    throw LinkletListenFailed();
  }
  
  // Log our listen attempt
  UNISPHERE_LOG(m_manager.context(), Info, "IPLinklet: Listening for incoming connections.");
  
  // Setup the TCP acceptor
  LinkletPtr linklet(new IPLinklet(m_manager));
  linklet->signalConnectionSuccess.connect(signalAcceptedConnection);
  m_acceptor.async_accept(
    boost::static_pointer_cast<IPLinklet>(linklet)->socket(),
    boost::bind(&IPLinklet::handleAccept, this, linklet, boost::asio::placeholders::error)
  );
}

void IPLinklet::connect(const Address &address)
{
  m_connectAddress = address;
  m_state = State::Connecting;
  
  // Log our connection attempt
  UNISPHERE_LOG(m_manager.context(), Info, "IPLinklet: Connecting to a remote address...");
  
  // Setup the TCP socket
  socket().async_connect(
    address.toIpEndpoint(),
    boost::bind(&IPLinklet::handleConnect, this, boost::asio::placeholders::error)
  );
}

void IPLinklet::close()
{
  if (m_state == State::Closed)
    return;
  
  UNISPHERE_LOG(m_manager.context(), Info, "IPLinklet: Closing connection.");
  State state = m_state;
  m_state = State::Closed;
  socket().close();
  
  // Emit the proper signal accoording to previous connection state
  if (state == State::Connected) {
    signalDisconnected(shared_from_this());
  } else {
    signalConnectionFailed(shared_from_this());
  }
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
  m_socket.async_read_some(
    boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
    boost::bind(&IPLinklet::handleReadHeader, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
  );
}

void IPLinklet::handleAccept(LinkletPtr linklet, const boost::system::error_code &error)
{
  if (!error) {
    // Start the connection
    boost::static_pointer_cast<IPLinklet>(linklet)->start(false);
    
    // Ready for the next connection
    LinkletPtr nextLinklet(new IPLinklet(m_manager));
    nextLinklet->signalConnectionSuccess.connect(signalAcceptedConnection);
    m_acceptor.async_accept(
      boost::static_pointer_cast<IPLinklet>(nextLinklet)->socket(),
      boost::bind(&IPLinklet::handleAccept, this, nextLinklet, boost::asio::placeholders::error)
    );
  } else {
    // TODO Error while accepting, how to handle this?
  }
}

void IPLinklet::handleConnect(const boost::system::error_code &error)
{
  if (!error) {
    // Connection successful
    UNISPHERE_LOG(m_manager.context(), Info, "IPLinklet: Outgoing connection successful."); 
    start();
  } else {
    // Signal connection failure to upper layers
    UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Outgoing connection failed!");
    signalConnectionFailed(shared_from_this());
  }
}

void IPLinklet::send(const Message &msg)
{
  if (m_state != State::Connected && msg.type() != Message::Type::Interplex_Hello)
    return;
  
  boost::lock_guard<boost::mutex> g(m_outMessagesMutex);
  bool sendInProgress = !m_outMessages.empty();
  m_outMessages.push_back(msg);
  
  if (!sendInProgress) {
    if (m_state == State::IntroWait) {
      boost::asio::async_write(
        m_socket,
        boost::asio::buffer(m_outMessages.front().buffer()),
        boost::bind(&IPLinklet::handleWrite, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
          boost::asio::placeholders::error)
      );
    } else {
      boost::asio::async_write(
        m_socket,
        boost::asio::buffer(m_outMessages.front().buffer()),
        boost::bind(&IPLinklet::handleWrite, this, boost::asio::placeholders::error)
      );
    }
  }
}

void IPLinklet::handleWrite(const boost::system::error_code &error)
{
  if (!error) {
    boost::lock_guard<boost::mutex> g(m_outMessagesMutex);
    m_outMessages.pop_front();
    if (!m_outMessages.empty()) {
      if (m_state == State::IntroWait) {
        boost::asio::async_write(
          m_socket,
          boost::asio::buffer(m_outMessages.front().buffer()),
          boost::bind(&IPLinklet::handleWrite, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
            boost::asio::placeholders::error)
        );
      } else {
        boost::asio::async_write(
          m_socket,
          boost::asio::buffer(m_outMessages.front().buffer()),
          boost::bind(&IPLinklet::handleWrite, this, boost::asio::placeholders::error)
        );
      }
    }
  } else {
    // Log
    UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Message write failed!");
    close();
  }
}

void IPLinklet::handleReadHeader(const boost::system::error_code &error, size_t bytes)
{
  if (!error && bytes == Message::header_size) {
    // Parse header and get message size
    size_t payloadSize = m_inMessage.parseHeader();
    if (payloadSize == 0) {
      handleReadPayload(error);
    } else {
      if (m_state == State::IntroWait) {
        // Only hello messages are allowed in IntroWait state
        if (m_inMessage.type() != Message::Type::Interplex_Hello) {
          UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Received non-hello message in IntroWait phase!");
          close();
          return;
        }
        
        // Read payload
        m_socket.async_read_some(
          boost::asio::buffer(&m_inMessage.buffer()[0] + Message::header_size, payloadSize),
          boost::bind(&IPLinklet::handleReadPayload, boost::static_pointer_cast<IPLinklet>(shared_from_this()),
            boost::asio::placeholders::error)
        );
      } else {
        // Read payload
        m_socket.async_read_some(
          boost::asio::buffer(&m_inMessage.buffer()[0] + Message::header_size, payloadSize),
          boost::bind(&IPLinklet::handleReadPayload, this, boost::asio::placeholders::error)
        );
      }
    }
  } else {
    // Log
    UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Message header read failed!");
    close();
  }
}

void IPLinklet::handleReadPayload(const boost::system::error_code &error)
{
  if (!error) {
    if (m_state == State::IntroWait) {
      // We have received the hello message, extract information and detach
      Protocol::Interplex::Hello hello = message_cast<Protocol::Interplex::Hello>(m_inMessage);
      Contact peerContact = Contact::fromMessage(hello.local_contact());
      if (peerContact.isNull()) {
        UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Peer node id mismatch in hello message!");
        return close();
      }
      
      UNISPHERE_LOG(m_manager.context(), Info, "IPLinklet: Introductory phase completed.");
      m_peerContact = peerContact;
      m_state = State::Connected;
      signalConnectionSuccess(shared_from_this());
      
      // The above signal may close this linklet if peer verification fails, in this case we
      // should not start reading, otherwise this will cause a segmentation fault
      if (m_state != State::Connected)
        return;
    } else {
      // Payload has been read, emit message and detach
      signalMessageReceived(shared_from_this(), m_inMessage);
    }
    
    m_inMessage.detach();
    
    // Ready for next message
    m_socket.async_read_some(
      boost::asio::buffer(m_inMessage.buffer(), Message::header_size),
      boost::bind(&IPLinklet::handleReadHeader, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    );
  } else {
    // Log
    UNISPHERE_LOG(m_manager.context(), Error, "IPLinklet: Message body read failed!");
    close();
  }
}

}
