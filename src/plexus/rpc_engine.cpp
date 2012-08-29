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
#include "plexus/rpc_engine.h"
#include "plexus/router.h"
#include "interplex/link_manager.h"

#include <botan/auto_rng.h>

namespace UniSphere {

RpcException::RpcException(RpcErrorCode code, const std::string &msg)
  : Exception("RPC Exception: " + msg),
    m_code(code),
    m_message(msg)
{
}
  
RpcCall::RpcCall(RpcEngine &rpc, RpcId rpcId, const NodeIdentifier &destination, RpcResponseSuccess success,
                 RpcResponseFailure failure, boost::posix_time::time_duration timeout)
  : m_rpc(rpc),
    m_rpcId(rpcId),
    m_destination(destination),
    m_strand(rpc.router().linkManager().context().service()),
    m_success(success),
    m_failure(failure),
    m_timer(rpc.router().linkManager().context().service()),
    m_timeout(timeout)
{
}

void RpcCall::start()
{
  RpcCallWeakPtr me(shared_from_this());
  m_timer.expires_from_now(m_timeout);
  m_timer.async_wait(m_strand.wrap([me](const boost::system::error_code&) {
    // We are using a weak reference, because the object might already be gone
    // when we come to this point and we need to check for this
    if (RpcCallPtr self = me.lock()) {
      self->cancel();
      if (self->m_failure)
        self->m_failure(RpcErrorCode::RequestTimedOut, "Request timed out.");
    }
  }));
}

void RpcCall::done(const Protocol::RpcResponse &response)
{
  RpcCallWeakPtr me(shared_from_this());
  
  // Response must be copied as a reference will go away after the method completes
  m_strand.post([me, response]() {
    if (RpcCallPtr self = me.lock()) {
      self->m_timer.cancel();
      self->cancel();
      if (self->m_success)
        self->m_success(response);
    }
  });
}

void RpcCall::cancel()
{
  m_rpc.cancel(m_rpcId);
}
  
RpcEngine::RpcEngine(Router &router)
  : m_router(router)
{
  // Subscribe to message delivery and forward events
  m_router.signalDeliverMessage.connect(boost::bind(&RpcEngine::messageDelivery, this, _1));
  m_router.signalForwardMessage.connect(boost::bind(&RpcEngine::messageForward, this, _1));
}

RpcId RpcEngine::getNextRpcId() const
{
  // TODO maybe we should unify the random generator!
  Botan::AutoSeeded_RNG rng;
  RpcId rpcId;
  rng.randomize((Botan::byte*) &rpcId, sizeof(rpcId));
  return rpcId;
}

RpcCallPtr RpcEngine::createCall(const NodeIdentifier &destination, const std::string &method,
                             const std::vector<char> &payload, RpcResponseSuccess success,
                             RpcResponseFailure failure, const RpcCallOptions &opts)
{
  // Register the pending RPC call
  RpcCallPtr call(new RpcCall(*this, getNextRpcId(), destination, success, failure, boost::posix_time::seconds(opts.timeout)));
  {
    RecursiveUniqueLock lock(m_mutex);
    m_pendingCalls[call->rpcId()] = call;
    m_recentCalls.push_front(call->rpcId());
    if (m_recentCalls.size() > RpcEngine::recent_size)
      m_recentCalls.pop_back();
  }
  call->start();
  
  // Prepare the request message
  Protocol::RpcRequest msg;
  msg.set_rpc_id(call->rpcId());
  msg.set_method(method);
  msg.set_data(&payload[0], payload.size());
  
  // Send the RPC message
  m_router.route(
    static_cast<std::uint32_t>(Router::Component::RPC_Engine),
    destination,
    static_cast<std::uint32_t>(Router::Component::RPC_Engine),
    static_cast<std::uint32_t>(RpcMessageType::Request),
    msg,
    opts.routingOptions
  );
  
  return call;
}

void RpcEngine::cancel(RpcId rpcId)
{
  RecursiveUniqueLock lock(m_mutex);
  m_pendingCalls.erase(rpcId);
}

Protocol::RpcResponse RpcEngine::getErrorResponse(RpcId rpcId, RpcErrorCode code, const std::string &message) const
{
  Protocol::RpcError error;
  error.set_code(static_cast<std::uint32_t>(code));
  error.set_message(message);
  
  Protocol::RpcResponse response;
  response.set_rpc_id(rpcId);
  response.set_error(true);
  
  std::vector<char> buffer(error.ByteSize());
  error.SerializeToArray(&buffer[0], buffer.size());
  response.set_data(&buffer[0], buffer.size());
  
  return response;
}

void RpcEngine::messageDelivery(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(Router::Component::RPC_Engine))
    return;
  
  switch (msg.payloadType()) {
    case static_cast<std::uint32_t>(RpcMessageType::Request): {
      Protocol::RpcRequest request = message_cast<Protocol::RpcRequest>(msg);
      RpcId rpcId = request.rpc_id();
      
      RecursiveUniqueLock lock(m_mutex);
      if (m_methods.find(request.method()) == m_methods.end())
        return respond(msg, getErrorResponse(rpcId, RpcErrorCode::MethodNotFound, "Method not found."));
      
      auto handler = m_methods[request.method()];
      lock.unlock();
      
      // Call the registered method handler
      handler(
        msg, request,
        [this, msg](const Protocol::RpcResponse &response) { respond(msg, response); },
        [this, msg, rpcId](RpcErrorCode code, const std::string &emsg) { respond(msg, getErrorResponse(rpcId, code, emsg)); }
      );
      break;
    }
    case static_cast<std::uint32_t>(RpcMessageType::Response): {
      RecursiveUniqueLock lock(m_mutex);
      Protocol::RpcResponse response = message_cast<Protocol::RpcResponse>(msg);
      RpcId rpcId = response.rpc_id();
      if (m_pendingCalls.find(rpcId) == m_pendingCalls.end())
        return;
      
      m_pendingCalls[rpcId]->done(response);
      break;
    }
  }
}

void RpcEngine::messageForward(const RoutedMessage &msg)
{
  if (msg.destinationCompId() != static_cast<std::uint32_t>(Router::Component::RPC_Engine) ||
      msg.payloadType() != static_cast<std::uint32_t>(RpcMessageType::Request))
    return;
  
  RecursiveUniqueLock lock(m_mutex);
  Protocol::RpcRequest request = message_cast<Protocol::RpcRequest>(msg);
  RpcId rpcId = request.rpc_id();
  
  if (m_interceptMethods.find(request.method()) == m_interceptMethods.end())
    return;
  
  auto handler = m_interceptMethods[request.method()];
  lock.unlock();
  
  // Call the registered method handler for the intercepted RPC request
  handler(
    msg, request,
    [](const Protocol::RpcResponse&) {},
    [](RpcErrorCode, const std::string&) {}
  );
}

void RpcEngine::respond(const RoutedMessage &msg, const Protocol::RpcResponse &response)
{
  // Send the RPC message back to the source node
  m_router.route(
    static_cast<std::uint32_t>(Router::Component::RPC_Engine),
    msg.sourceNodeId(),
    static_cast<std::uint32_t>(Router::Component::RPC_Engine),
    static_cast<std::uint32_t>(RpcMessageType::Response),
    response
  );
}

bool RpcEngine::isRecentCall(RpcId rpcId)
{
  return m_recentCalls.get<1>().find(rpcId) != m_recentCalls.get<1>().end();
}

}
