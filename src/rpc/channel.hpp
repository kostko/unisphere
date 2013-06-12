/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <k@jst.sm>
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
#ifndef UNISPHERE_RPC_CHANNEL_H
#define UNISPHERE_RPC_CHANNEL_H

#include "core/globals.h"

#include <boost/signals2/signal.hpp>

namespace UniSphere {

class Context;

template <typename Impl>
class RpcChannel {
public:
  RpcChannel(Context &context);

  Context &context() const { return m_context; }

  virtual void respond(const typename Impl::message_type &msg,
                       const Protocol::RpcResponse &response,
                       const typename Impl::options_type &opts) = 0;

  virtual void request(const NodeIdentifier &destination,
                       const Protocol::RpcRequest &request,
                       const typename Impl::options_type &opts) = 0;
public:
  /// Signal that gets called when a new request has to be processed by the RPC engine
  boost::signals2::signal<void(const Protocol::RpcRequest&, typename Impl::message_type&)> signalDeliverRequest;
  /// Signal that gets called when a new response has to be processed by the RPC engine
  boost::signals2::signal<void(const Protocol::RpcResponse&, typename Impl::message_type&)> signalDeliverResponse;
private:
  /// UNISPHERE context instance
  Context &m_context;
};

}

#endif
