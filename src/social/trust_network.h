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
#ifndef UNISPHERE_SOCIAL_TRUSTNETWORK_H
#define UNISPHERE_SOCIAL_TRUSTNETWORK_H

#include "core/globals.h"
#include "identity/node_identifier.h"

#include <boost/signals.hpp>

namespace UniSphere {

/**
 * An interface for trust network implementations. A trust network can be
 * used to request credit flow computations that will verify if enough credit
 * exists on the path to some destination node.
 */
class UNISPHERE_EXPORT TrustNetwork {
public:
  /**
   * Requests a computation of the credit flow from the local node to some
   * specific peer. If the credit operation is successful this will also
   * modify the credit graph. This is an asynchronous operation the result
   * of which is signaled via @ref signalCreditComputed.
   * 
   * @param destination Destination peer identifier
   * @param credit Amount to credit the peer for
   */
  virtual void requestCreditFlow(const NodeIdentifier &destination, double credit) = 0;
public:
  /// Signal that gets emitted when a new credit flow computation is completed
  boost::signal<void(const NodeIdentifier&, bool)> signalCreditComputed;
};

}

#endif
