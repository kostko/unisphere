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
#include "interplex/linklet.h"
#include "interplex/link_manager.h"
#include "src/interplex/interplex.pb.h"

#include <boost/log/attributes/constant.hpp>

namespace UniSphere {

Linklet::Linklet(LinkManager &manager)
  : m_manager(manager),
    m_service(manager.context().service()),
    m_strand(m_service),
    m_state(State::Closed),
    m_logger(logging::keywords::channel = "linklet")
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(manager.getLocalNodeId()));
}

Linklet::~Linklet()
{
}

bool Linklet::headerParsed(Message &msg)
{
  if (m_state == State::IntroWait) {
    // Only hello messages are allowed in IntroWait state
    if (msg.type() != Message::Type::Interplex_Hello) {
      BOOST_LOG_SEV(m_logger, log::error) << "Received non-hello message in IntroWait phase!";
      return false;
    }
  }

  return true;
}

bool Linklet::messageParsed(Message &msg)
{
  if (m_state == State::IntroWait) {
    // We have received the hello message, extract information and detach
    Protocol::Interplex::Hello hello = message_cast<Protocol::Interplex::Hello>(msg);
    Contact peerContact = Contact::fromMessage(hello.local_contact());
    if (peerContact.isNull()) {
      BOOST_LOG_SEV(m_logger, log::error) << "Invalid peer contact in hello message!";
      return false;
    }

    m_peerContact = peerContact;

    // Perform additional verification on the peer before transitioning into
    // the connected state
    if (!signalVerifyPeer(shared_from_this()) || !m_manager.verifyPeer(peerContact)) {
      return false;
    }

    BOOST_LOG_SEV(m_logger, log::normal) << "Introductory phase with " << peerContact.nodeId().hex() << " completed.";
    m_state = State::Connected;
    signalConnectionSuccess(shared_from_this());

    // The above signal may close this linklet, in this case we should not start reading,
    // otherwise this will cause a segmentation fault
    if (m_state != State::Connected)
      return false;
  } else {
    // Payload has been read, emit message and detach
    signalMessageReceived(shared_from_this(), msg);
  }

  msg.detach();
  return true;
}

}
