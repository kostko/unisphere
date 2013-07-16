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
#ifndef UNISPHERE_RPC_ENGINE_H
#define UNISPHERE_RPC_ENGINE_H

#include "rpc/call.hpp"
#include "rpc/call_group.hpp"
#include "rpc/options.hpp"
#include "rpc/service_fwd.hpp"
#include "src/rpc/rpc.pb.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <unordered_map>

namespace UniSphere {

/**
 * This class handles RPC calls between nodes. Each RPC call is composed of
 * two parts - request and response, both formatted as Protocol Buffers
 * messages.
 *
 * The engine implementation is generic, a channel implementation has to
 * provide transport functions.
 */
template <typename Channel>
class UNISPHERE_EXPORT RpcEngine {
public:
  /// Recent RPC call list size
  static const size_t recent_size = 20;
  
  /**
   * Class constructor.
   * 
   * @param channel Channel instance
   */
  explicit RpcEngine(Channel &channel)
    : m_context(channel.context()),
      m_logger(logging::keywords::channel = "rpc_engine"),
      m_channel(channel)
  {
    // Subscribe to message delivery events
    m_channel.signalDeliverRequest.connect(boost::bind(&RpcEngine<Channel>::handleRequest, this, _1, _2));
    m_channel.signalDeliverResponse.connect(boost::bind(&RpcEngine<Channel>::handleResponse, this, _1, _2));
  }

  RpcEngine(const RpcEngine&) = delete;
  RpcEngine &operator=(const RpcEngine&) = delete;

  /**
   * Returns the context instance.
   */
  Context &context() const { return m_context; }
  
  /**
   * Returns the channel instance associated with this RPC engine.
   */
  Channel &channel() const { return m_channel; }

  /**
   * Returns the logger instance associated with this RPC engine.
   */
  Logger &logger() { return m_logger; }

  /**
   * Starts an RPC call group.
   *
   * @param complete Completion handler that gets invoked when all grouped
   *   calls are completed
   */
  RpcCallGroupPtr<Channel> group(RpcGroupCompletionHandler complete)
  {
    return RpcCallGroupPtr<Channel>(new RpcCallGroup<Channel>(*this, complete));
  }

  /**
   * Creates a new service instance.
   *
   * @param destination Destination identifier
   * @param opts Channel-specific options
   */
  RpcService<Channel> service(const NodeIdentifier &destination,
                              const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
  {
    return RpcService<Channel>(*this, destination, opts);
  }

  /**
   * Returns a new RPC call options instance.
   */
  RpcCallOptions<Channel> options() { return RpcCallOptions<Channel>(); }
  
  /**
   * Calls a remote procedure.
   *
   * @param destination Destination key
   * @param method Method name
   * @param request Request payload
   * @param success Success callback
   * @param failure Failure callback
   * @param opts Call options
   */
  template<typename RequestType, typename ResponseType>
  void call(const NodeIdentifier &destination,
            const std::string &method,
            const RequestType &request,
            std::function<void(const ResponseType&, const typename Channel::message_type&)> success,
            RpcResponseFailure failure = nullptr,
            const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
  {
    // Serialize Protocol Buffers message into the payload
    std::vector<char> buffer(request.ByteSize());
    request.SerializeToArray(&buffer[0], buffer.size());
    
    createCall(destination, method, buffer,
      [success](const Protocol::RpcResponse &rsp, const typename Channel::message_type &msg) {
        if (success)
          success(message_cast<ResponseType>(rsp.data()), msg);
      },
      failure,
      opts
    );
  }
  
  /**
   * Calls a remote procedure without confirmation.
   *
   * @param destination Destination key
   * @param method Method name
   * @param request Request payload
   * @param opts Call options
   */
  template<typename RequestType>
  void call(const NodeIdentifier &destination,
            const std::string &method,
            const RequestType &request = RequestType(),
            const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
  {
    // Serialize Protocol Buffers message into the payload
    std::vector<char> buffer(request.ByteSize());
    request.SerializeToArray(&buffer[0], buffer.size());
    
    // Create the call and immediately cancel it as we don't need a confirmation
    RpcCallPtr<Channel> call = createCall(destination, method, buffer, nullptr, nullptr, opts);
    call->cancel();
  }
  
  /**
   * Cancels a given pending RPC call.
   * 
   * @param rpcId Call's unique identifier
   */
  void cancel(RpcId rpcId)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_pendingCalls.erase(rpcId);
  }
  
  /**
   * Verifies that the specific RPC call was an actual recent outgoing call performed
   * by this node.
   */
  bool isRecentCall(RpcId rpcId) const
  {
    return m_recentCalls.get<1>().find(rpcId) != m_recentCalls.get<1>().end();
  }
  
  /**
   * Registers a new RPC method call.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType>
  void registerMethod(const std::string &method,
                      std::function<RpcResponse<Channel, ResponseType>(const RequestType&, const typename Channel::message_type&, RpcId rpcId)> impl)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_methods[method] = createBasicMethodHandler<RequestType, ResponseType>(method, impl);
  }
  
  /**
   * Registers a new RPC method call that doesn't send back a response.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType>
  void registerMethod(const std::string &method,
                      std::function<void(const RequestType&, const typename Channel::message_type&, RpcId rpcId)> impl)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_methods[method] = createBasicMethodHandler<RequestType>(method, impl);
  }

  /**
   * Removes an already registered method.
   *
   * @param method Method name
   */
  void unregisterMethod(const std::string &method)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_methods.erase(method);
  }
protected:
  /**
   * Generates a new RPC identifier.
   */
  RpcId getNextRpcId() const
  {
    RpcId rpcId;
    m_channel.context().rng().randomize((Botan::byte*) &rpcId, sizeof(rpcId));
    return rpcId;
  }
  
  /**
   * Creates a new pending RPC call descriptor and submits the message via
   * the router.
   * 
   * @param destination Destination key
   * @param method Method name
   * @param payload Request payload
   * @param success Success callback
   * @param failure Failure callback
   * @param opts Call options
   */
  RpcCallPtr<Channel> createCall(const NodeIdentifier &destination,
                                 const std::string &method,
                                 const std::vector<char> &payload,
                                 RpcCallSuccess<Channel> success,
                                 RpcResponseFailure failure,
                                 const RpcCallOptions<Channel> &opts)
  {
    // Register the pending RPC call
    RpcCallPtr<Channel> call(new RpcCall<Channel>(*this, getNextRpcId(), destination, success, failure, boost::posix_time::seconds(opts.timeout)));
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
    m_channel.request(destination, msg, opts.channelOptions);
    return call;
  }
  
  /**
   * Creates a new RPC method handler.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType>
  RpcHandler<Channel> createBasicMethodHandler(const std::string &method,
                                               std::function<RpcResponse<Channel, ResponseType>(
                                                 const RequestType&,
                                                 const typename Channel::message_type&,
                                                 RpcId
                                               )> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const typename Channel::message_type &msg,
                  const Protocol::RpcRequest &request,
                  RpcResponseSuccess<Channel> success,
                  RpcResponseFailure failure) {
      try {
        // Deserialize the message and call method implementation
        RpcResponse<Channel, ResponseType> rsp = impl(message_cast<RequestType>(request.data()), msg, request.rpc_id());
        Protocol::RpcResponse response;
        response.set_rpc_id(request.rpc_id());
        response.set_error(false);
        
        // Serialize response message into the payload
        std::vector<char> buffer(rsp.response.ByteSize());
        rsp.response.SerializeToArray(&buffer[0], buffer.size());
        response.set_data(&buffer[0], buffer.size());
        success(response, rsp.channelOptions);
      } catch (RpcException &error) {
        // Handle failures by invoking the failure handler
        failure(error.code(), error.message());
      }
    };
  }
  
  /**
   * Creates a new RPC method handler.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType>
  RpcHandler<Channel> createBasicMethodHandler(const std::string &method,
                                               std::function<void(
                                                 const RequestType&,
                                                 const typename Channel::message_type&,
                                                 RpcId
                                               )> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const typename Channel::message_type &msg,
                  const Protocol::RpcRequest &request,
                  RpcResponseSuccess<Channel> success,
                  RpcResponseFailure failure) {
      try {
        // Deserialize the message and call method implementation
        impl(message_cast<RequestType>(request.data()), msg, request.rpc_id());
      } catch (RpcException &error) {
        // Handle failures by invoking the failure handler
        failure(error.code(), error.message());
      }
    };
  }
  
  /**
   * Generates an error response for an RPC call.
   * 
   * @param rpcId RPC call identifier
   * @param code Error code
   * @param message Error message
   * @return A proper RpcResponse with an error descriptor as the payload
   */
  Protocol::RpcResponse getErrorResponse(RpcId rpcId,
                                         RpcErrorCode code,
                                         const std::string &message) const
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
protected:
  /**
   * Called by the channel when a request has been received.
   *
   * @param request RPC request message
   * @param msg Channel-specific source message
   */
  void handleRequest(const Protocol::RpcRequest &request,
                     const typename Channel::message_type &msg)
  {
    RecursiveUniqueLock lock(m_mutex);
    RpcId rpcId = request.rpc_id();
    if (m_methods.find(request.method()) == m_methods.end())
      return m_channel.respond(msg, getErrorResponse(rpcId, RpcErrorCode::MethodNotFound, "Method not found."));
    
    auto handler = m_methods[request.method()];
    lock.unlock();
    
    // Call the registered method handler
    handler(
      msg, request,
      [this, msg](const Protocol::RpcResponse &response, const typename Channel::options_type &opts)
      {
        m_channel.respond(msg, response, opts);
      },
      [this, msg, rpcId](RpcErrorCode code, const std::string &emsg)
      {
        m_channel.respond(msg, getErrorResponse(rpcId, code, emsg));
      }
    );
  }

  /**
   * Called by the channel when a response has been received.
   *
   * @param request RPC response message
   * @param msg Channel-specific source message
   */
  void handleResponse(const Protocol::RpcResponse &response,
                      const typename Channel::message_type &msg)
  {
    RecursiveUniqueLock lock(m_mutex);
    RpcId rpcId = response.rpc_id();
    if (m_pendingCalls.find(rpcId) == m_pendingCalls.end()) {
      BOOST_LOG_SEV(m_logger, log::warning) << "Got RPC response for an unknown call!";
      return;
    }
    
    m_pendingCalls[rpcId]->done(response, msg);
  }
private:  
  /// UNISPHERE context
  Context &m_context;
  /// Logger instance
  Logger m_logger;
  /// Channel over which the RPCs are routed
  Channel &m_channel;
  /// Mutex protecting the RPC engine
  std::recursive_mutex m_mutex;
  /// Pending RPC calls
  std::unordered_map<RpcId, RpcCallPtr<Channel>> m_pendingCalls;
  /// Registered RPC methods
  std::unordered_map<std::string, RpcHandler<Channel>> m_methods;
  /// Recent RPC calls
  boost::multi_index_container<
    RpcId,
    boost::multi_index::indexed_by<
      boost::multi_index::sequenced<>,
      boost::multi_index::hashed_unique<boost::multi_index::identity<RpcId>>
    >
  > m_recentCalls;
};

}

#endif
