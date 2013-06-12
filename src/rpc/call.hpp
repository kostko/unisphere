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
#ifndef UNISPHERE_RPC_CALL_H
#define UNISPHERE_RPC_CALL_H

#include "rpc/exceptions.h"
#include "rpc/channel.hpp"
#include "src/rpc/rpc.pb.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time.hpp>
#include <boost/asio.hpp>

namespace UniSphere {

/// RPC identifier type
typedef std::uint64_t RpcId;
/// RPC call mapping key
typedef std::tuple<NodeIdentifier, RpcId> RpcCallKey;

/// Callback type for successful RPC calls
template <typename Channel>
using RpcCallSuccess = std::function<void(const Protocol::RpcResponse&, const typename Channel::message_type&>;
/// Callback type for failed RPC calls
typedef std::function<void(RpcErrorCode, const std::string&)> RpcResponseFailure;
/// Callback type for RPC method handlers
template <typename Channel>
using RpcHandler = std::function<void(const typename Channel::message_type&, const Protocol::RpcRequest&, RpcResponseSuccess, RpcResponseFailure)>;

template<typename Channel>
class RpcCall<Channel>;

template <typename Channel>
using RpcCallPtr = boost::shared_ptr<RpcCall<Channel>>;

template <typename Channel>
using RpcCallWeakPtr = boost::weak_ptr<RpcCall<Channel>>;

/**
 * Descriptor for tracking pending RPC calls.
 */
template <typename Channel>
class UNISPHERE_EXPORT RpcCall : public boost::enable_shared_from_this<RpcCall<Channel>> {
public:
  /**
   * Constructs an RPC call.
   * 
   * @param rpc RPC engine that created this call
   * @param rpcId Call's unique identifier
   * @param destination Destination key identifier
   * @param success Success handler
   * @param failure Failure handler
   * @param timeout Timeout
   */
  RpcCall(RpcEngine<Channel> &rpc,
          RpcId rpcId,
          const NodeIdentifier &destination,
          RpcCallSuccess<Channel> success,
          RpcResponseFailure failure,
          boost::posix_time::time_duration timeout)
    : m_rpc(rpc),
      m_rpcId(rpcId),
      m_destination(destination),
      m_finished(false),
      m_strand(rpc.context().service()),
      m_success(success),
      m_failure(failure),
      m_timer(rpc.context().service()),
      m_timeout(timeout)
  {
  }
  
  RpcCall(const RpcCall&) = delete;
  RpcCall &operator=(const RpcCall&) = delete;
  
  /**
   * Returns the unique identifier of this RPC call.
   */
  RpcId rpcId() const { return m_rpcId; }
  
  /**
   * Returns the destination key for this RPC call.
   */
  NodeIdentifier destination() const { return m_destination; }
  
  /**
   * Dispatches the RPC request and starts the timeout timer.
   */
  void start()
  {
    RpcCallWeakPtr<Channel> me(shared_from_this());
    m_timer.expires_from_now(m_timeout);
    m_timer.async_wait(m_strand.wrap([me](const boost::system::error_code&) {
      // We are using a weak reference, because the object might already be gone
      // when we come to this point and we need to check for this
      if (RpcCallPtr<Channel> self = me.lock()) {
        if (self->m_finished)
          return;

        self->cancel();
        if (self->m_failure)
          self->m_failure(RpcErrorCode::RequestTimedOut, "Request timed out.");
      }
    }));
  }
  
  /**
   * Signals the successful receipt of an RPC response.
   * 
   * @param response RPC response
   * @param msg Channel-specific lower-layer message
   */
  void done(const Protocol::RpcResponse &response, const typename Channel::message_type &msg)
  {
    RpcCallWeakPtr<Channel> me(shared_from_this());
    
    // Response must be copied as a reference will go away after the method completes
    m_strand.post([me, response, meta]() {
      if (RpcCallPtr<Channel> self = me.lock()) {
        if (self->m_finished)
          return;

        self->m_timer.cancel();
        self->cancel();
        if (self->m_success)
          self->m_success(response, meta);
      }
    });
  }
  
  /**
   * Cancels this call and doesn't call the failure handler.
   */
  void cancel()
  {
    m_finished = true;
    m_rpc.cancel(m_rpcId);
  }
private:
  /// RPC engine that generated this call
  RpcEngine<Channel> &m_rpc;
  /// Unique identifier for this RPC call
  RpcId m_rpcId;
  /// Destination key for this RPC call
  NodeIdentifier m_destination;
  /// Status of this call
  bool m_finished;
  
  /// Strand to ensure that success and failure handlers are
  /// always executed serially
  boost::asio::strand m_strand;
  
  /// Timer for detecting lost messages
  boost::asio::deadline_timer m_timer;
  /// Timeout
  boost::posix_time::time_duration m_timeout;
  
  /// RPC success handler
  RpcCallSuccess<Channel> m_success;
  /// RPC failure handler
  RpcResponseFailure m_failure;
};

/**
 * Class for returning responses to RPC method calls.
 */
template <typename Channel, typename ResponseType>
class UNISPHERE_EXPORT RpcResponse {
public:
  /**
   * Constructor for implicitly converting from response types without
   * specifying any options.
   * 
   * @param response Response message
   */
  RpcResponse(ResponseType rsp)
    : response(rsp)
  {}
  
  /**
   * Constructor for defining routing options.
   * 
   * @param response Response message
   * @param opts Channel-specific options
   */
  RpcResponse(ResponseType rsp, const typename Channel::options_type &opts)
    : response(rsp),
      routingOptions(opts)
  {}
public:
  /// The actual response message
  ResponseType response;
  /// Routing options
  typename Channel::options_type routingOptions;
};

}

#endif
