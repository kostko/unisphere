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
#ifndef UNISPHERE_INTERPLEX_LINKMANAGER_H
#define UNISPHERE_INTERPLEX_LINKMANAGER_H

#include "core/context.h"
#include "core/signal.h"
#include "interplex/contact.h"
#include "interplex/linklet_factory.h"
#include "interplex/message.h"

#include <unordered_map>
#include <boost/signals2/signal.hpp>

namespace UniSphere {

UNISPHERE_SHARED_POINTER(Link)

/**
 * A link manager is used to manage links to all peers in a unified way.
 */
class UNISPHERE_EXPORT LinkManager {
public:
  friend class Link;

  /**
   * A structure for reporting link manager statistics.
   */
  struct Statistics {
    struct LinkStatistics {
      /// Number of transmitted messages
      size_t msgXmits = 0;
      /// Number of received messages
      size_t msgRcvd = 0;
    };

    /// Global statistics
    LinkStatistics global;
    /// Per-link statistics
    std::unordered_map<NodeIdentifier, LinkStatistics> links;
  };

  /**
   * Constructs a new link manager instance.
   *
   * @param context UNISPHERE context
   * @param privateKey Private key of the local node
   */
  LinkManager(Context &context, const PrivatePeerKey &privateKey);

  LinkManager(const LinkManager&) = delete;
  LinkManager &operator=(const LinkManager&) = delete;

  /**
   * Returns the UNISPHERE context this manager belongs to.
   */
  Context &context() const;

  /**
   * Sends a message to the given contact address.
   *
   * @param contact Destination node's contact information
   * @param msg Message to send
   */
  void send(const Contact &contact, const Message &msg);

  /**
   * Sends a message to the given peer. If there is no existing link
   * for the specified peer the message will not be delivered.
   *
   * @param nodeId Destination node's identifier
   * @param msg Message to send
   */
  void send(const NodeIdentifier &nodeId, const Message &msg);

  /**
   * Opens a listening linklet.
   *
   * @param address Local address to listen on
   * @return True if listen was successful
   */
  bool listen(const Address &address);

  /**
   * Closes all existing links and stops listening for any new ones.
   */
  void close();

  /**
   * Returns the contact for a given link identifier.
   *
   * @param linkId Link identifier
   * @return Contact for the specified link
   */
  Contact getLinkContact(const NodeIdentifier &linkId);

  /**
   * Returns a list of node identifiers to links that we have established.
   */
  std::list<NodeIdentifier> getLinkIds();

  /**
   * Returns the linklet factory instance.
   */
  const LinkletFactory &getLinkletFactory() const;

  /**
   * Returns the local contact information.
   */
  Contact getLocalContact() const;

  /**
   * Returns the local node identifier.
   */
  const NodeIdentifier &getLocalNodeId() const;

  /**
   * Returns the local private key.
   */
  const PrivatePeerKey &getLocalPrivateKey() const;

  /**
   * Sets a local address for all outgoing connections. This will cause all
   * outgoing sockets to bind to this address.
   *
   * @param address Local outgoing address
   */
  void setLocalAddress(const Address &address);

  /**
   * Returns the local address.
   */
  const Address &getLocalAddress() const;

  /**
   * Invokes registered peer verification hooks (signalVerifyPeer) and
   * returns the result.
   *
   * @param contact Contact to verify
   * @return True if verification was successful, false otherwise
   */
  bool verifyPeer(const Contact &contact);

  /**
   * Retrieves various statistics about link manager operation.
   */
  const Statistics &statistics() const;
public:
  /// Signal for received messages
  boost::signals2::signal<void (const Message&)> signalMessageReceived;
  /// Signal for additional peer verification
  boost::signals2::signal<bool (const Contact&), AllTrueCombiner> signalVerifyPeer;
protected:
  /**
   * Removes a specific link.
   *
   * @param link Link instance
   */
  void removeLink(LinkPtr link);
private:
  UNISPHERE_DECLARE_PRIVATE(LinkManager)
};

}

#endif
