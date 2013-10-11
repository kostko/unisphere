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
#ifndef UNISPHERE_RPC_OPTIONS_H
#define UNISPHERE_RPC_OPTIONS_H

#include "core/globals.h"

namespace UniSphere {

/**
 * RPC call options can be used to specify per-call options.
 */
template <typename Channel>
class UNISPHERE_EXPORT RpcCallOptions {
public:
  /**
   * Constructs default options.
   */
  RpcCallOptions()
    : timeout(15)
  {}
  
  /**
   * Sets up channel-specific options for this RPC call.
   */
  RpcCallOptions<Channel> &setChannelOptions(const typename Channel::options_type &opts)
  {
    channelOptions = opts;
    return *this;
  }
  
  /**
   * Sets this call's timeout in seconds.
   */
  RpcCallOptions<Channel> &setTimeout(int seconds)
  {
    timeout = seconds;
    return *this;
  }
public:
  /// Timeout in seconds
  int timeout;
  /// Channel-specific options
  typename Channel::options_type channelOptions;
};

}

#endif
