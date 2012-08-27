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
#ifndef UNISPHERE_PLEXUS_RPCENGINE_H
#define UNISPHERE_PLEXUS_RPCENGINE_H

#include "core/globals.h"
#include "core/exception.h"
#include "plexus/routed_message.h"
#include "src/plexus/rpc.pb.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time.hpp>
#include <boost/asio.hpp>
#include <unordered_map>

namespace UniSphere {

/// RPC identifier type
typedef std::uint64_t RpcId;
/// RPC call mapping key
typedef std::tuple<NodeIdentifier, RpcId> RpcCallKey;

class RpcEngine;
class Router;

/**
 * RPC message types.
 */
enum class RpcMessageType : std::uint32_t {
  Request   = 0x01,
  Response  = 0x02,
};

/**
 * RPC error codes.
 */
enum class RpcErrorCode : std::uint32_t {
  MethodNotFound  = 0x01,
  RequestTimedOut = 0x02,
};

/// Callback type for successful RPC calls
typedef std::function<void(const Protocol::RpcResponse&)> RpcResponseSuccess;
/// Callback type for failed RPC calls
typedef std::function<void(RpcErrorCode, const std::string&)> RpcResponseFailure;
/// Callback type for RPC method handlers
typedef std::function<void(const RoutedMessage&, const Protocol::RpcRequest&, RpcResponseSuccess, RpcResponseFailure)> RpcHandler;

/**
 * An RPC exception can be raised by RPC method implementations and
 * cause an error message to be sent back as a reply.
 */
class UNISPHERE_EXPORT RpcException : public Exception {
public:
  /**
   * Constructs an RPC exception.
   * 
   * @param code Error code
   * @param msg Error message
   */
  RpcException(RpcErrorCode code, const std::string &msg = "");
  
  /**
   * Class desctructor.
   */
  ~RpcException() noexcept {};
  
  /**
   * Returns the error code.
   */
  inline RpcErrorCode code() const { return m_code; }
  
  /**
   * Returns the error message.
   */
  inline const std::string &message() const { return m_message; }
private:
  /// RPC error code
  RpcErrorCode m_code;
  /// RPC error message
  std::string m_message;
};

/**
 * Descriptor for tracking pending RPC calls.
 */
class UNISPHERE_NO_EXPORT RpcCall : public boost::enable_shared_from_this<RpcCall> {
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
  RpcCall(RpcEngine &rpc, RpcId rpcId, const NodeIdentifier &destination, RpcResponseSuccess success,
    RpcResponseFailure failure, boost::posix_time::time_duration timeout);
  
  RpcCall(const RpcCall&) = delete;
  RpcCall &operator=(const RpcCall&) = delete;
  
  /**
   * Returns the unique identifier of this RPC call.
   */
  inline RpcId rpcId() const { return m_rpcId; }
  
  /**
   * Returns the destination key for this RPC call.
   */
  inline NodeIdentifier destination() const { return m_destination; }
  
  /**
   * Dispatches the RPC request and starts the timeout timer.
   */
  void start();
  
  /**
   * Signals the successful receipt of an RPC response.
   * 
   * @param response RPC response
   */
  void done(const Protocol::RpcResponse &response);
  
  /**
   * Cancels this call and doesn't call the failure handler.
   */
  void cancel();
private:
  /// RPC engine that generated this call
  RpcEngine &m_rpc;
  /// Unique identifier for this RPC call
  RpcId m_rpcId;
  /// Destination key for this RPC call
  NodeIdentifier m_destination;
  
  /// Strand to ensure that success and failure handlers are
  /// always executed serially
  boost::asio::strand m_strand;
  
  /// Timer for detecting lost messages
  boost::asio::deadline_timer m_timer;
  /// Timeout
  boost::posix_time::time_duration m_timeout;
  
  /// RPC success handler
  RpcResponseSuccess m_success;
  /// RPC failure handler
  RpcResponseFailure m_failure;
};

UNISPHERE_SHARED_POINTER(RpcCall);

/**
 * This class handles RPC calls between nodes. Each RPC call is composed of
 * two parts - request and response, both formatted as Protocol Buffers
 * messages.
 */
class UNISPHERE_EXPORT RpcEngine {
public:
  /**
   * Class constructor.
   * 
   * @param router Router instance
   */
  explicit RpcEngine(Router &router);
  
  RpcEngine(const RpcEngine&) = delete;
  RpcEngine &operator=(const RpcEngine&) = delete;
  
  /**
   * Returns the router instance associated with this RPC engine.
   */
  inline Router &router() const { return m_router; }
  
  /**
   * Calls a remote procedure.
   *
   * @param destination Destination key
   * @param method Method name
   * @param request Request payload
   * @param success Success callback
   * @param failure Failure callback
   * @param timeout Timeout (in seconds)
   */
  template<typename RequestType, typename ResponseType>
  void call(const NodeIdentifier &destination, const std::string &method,
            const RequestType &request, std::function<void(const ResponseType&)> success,
            RpcResponseFailure failure = nullptr, int timeout = 15)
  {
    // Serialize Protocol Buffers message into the payload
    std::vector<char> buffer(request.ByteSize());
    request.SerializeToArray(&buffer[0], buffer.size());
    
    createCall(destination, method, buffer,
      [success](const Protocol::RpcResponse &rsp) { success(message_cast<ResponseType>(rsp.data())); },
      failure, timeout
    );
  }
  
  /**
   * Cancels a given pending RPC call.
   * 
   * @param destination Call's destination key
   * @param rpcId Call's unique identifier
   */
  void cancel(const NodeIdentifier &destination, RpcId rpcId);
  
  /**
   * Registers a new RPC method call.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType = void>
  void registerMethod(const std::string &method, std::function<ResponseType(const RequestType&, const RoutedMessage&)> impl)
  {
    UniqueLock lock(m_mutex);
    m_methods[method] = createBasicMethodHandler<RequestType, ResponseType>(method, impl);
  }
  
  /**
   * Registers a new RPC method interception call. These calls get invoked when
   * specific messages are forwarded (not delivered) via the local node. Responses
   * generated by such methods are ignored.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType>
  void registerInterceptMethod(const std::string &method, std::function<void(const RequestType&, const RoutedMessage&)> impl)
  {
    UniqueLock lock(m_mutex);
    m_interceptMethods[method] = createBasicMethodHandler<RequestType>(method, impl);
  }
protected:
  /**
   * Generates a new RPC identifier.
   */
  RpcId getNextRpcId() const;
  
  /**
   * Creates a new pending RPC call descriptor and submits the message via
   * the router.
   * 
   * @param destination Destination key
   * @param method Method name
   * @param payload Request payload
   * @param success Success callback
   * @param failure Failure callback
   * @param timeout Timeout (in seconds)
   */
  RpcCallPtr createCall(const NodeIdentifier &destination, const std::string &method,
                    const std::vector<char> &payload, RpcResponseSuccess success,
                    RpcResponseFailure failure, int timeout);
  
  /**
   * Creates a new RPC method handler.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType>
  RpcHandler createBasicMethodHandler(const std::string &method, std::function<ResponseType(const RequestType&, const RoutedMessage&)> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const RoutedMessage &msg, const Protocol::RpcRequest &request,
                  RpcResponseSuccess success, RpcResponseFailure failure) {
      try {
        // Deserialize the message and call method implementation
        ResponseType rsp = impl(message_cast<RequestType>(request.data()), msg);
        Protocol::RpcResponse response;
        response.set_rpc_id(request.rpc_id());
        
        // Serialize response message into the payload
        std::vector<char> buffer(rsp.ByteSize());
        rsp.SerializeToArray(&buffer[0], buffer.size());
        response.set_data(&buffer[0], buffer.size());
        success(response);
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
  RpcHandler createBasicMethodHandler(const std::string &method, std::function<void(const RequestType&, const RoutedMessage&)> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const RoutedMessage &msg, const Protocol::RpcRequest &request,
                  RpcResponseSuccess success, RpcResponseFailure failure) {
      try {
        // Deserialize the message and call method implementation
        impl(message_cast<RequestType>(request.data()), msg);
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
  Protocol::RpcResponse getErrorResponse(RpcId rpcId, RpcErrorCode code,
                                         const std::string &message) const;
  
  /**
   * Sends a response message back to the originator.
   * 
   * @param msg Original request message
   * @param response Response message
   */
  void respond(const RoutedMessage &msg, const Protocol::RpcResponse &response);
protected:
  /**
   * Called by the router when a message is to be delivered to the local
   * node.
   */
  void messageDelivery(const RoutedMessage &msg);
  
  /**
   * Called by the router when a message is to be forwarded via the local
   * node.
   */
  void messageForward(const RoutedMessage &msg);
private:
  /// Router over which the RPCs are routed
  Router &m_router;
  /// Mutex protecting the RPC engine
  std::mutex m_mutex;
  /// Pending RPC calls
  std::unordered_map<RpcCallKey, RpcCallPtr> m_pendingCalls;
  /// Registered RPC methods
  std::unordered_map<std::string, RpcHandler> m_methods;
  /// Registered RPC intercept methods
  std::unordered_map<std::string, RpcHandler> m_interceptMethods;
};
  
}

#endif
