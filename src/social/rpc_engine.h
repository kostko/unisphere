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
#ifndef UNISPHERE_SOCIAL_RPCENGINE_H
#define UNISPHERE_SOCIAL_RPCENGINE_H

#include "core/globals.h"
#include "core/exception.h"
#include "social/routed_message.h"
#include "src/social/rpc.pb.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time.hpp>
#include <boost/asio.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <unordered_map>

namespace midx = boost::multi_index;

namespace UniSphere {

/// RPC identifier type
typedef std::uint64_t RpcId;
/// RPC call mapping key
typedef std::tuple<NodeIdentifier, RpcId> RpcCallKey;

class RpcEngine;
class CompactRouter;

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
  BadRequest      = 0x03,
  NoAuthorization = 0x04,
};

/// Callback type for successful RPC method responses
typedef std::function<void(const Protocol::RpcResponse&, const RoutingOptions&)> RpcResponseSuccess;
/// Callback type for successful RPC calls
typedef std::function<void(const Protocol::RpcResponse&, const RoutedMessage&)> RpcCallSuccess;
/// Callback type for failed RPC calls
typedef std::function<void(RpcErrorCode, const std::string&)> RpcResponseFailure;
/// Callback type for RPC method handlers
typedef std::function<void(const RoutedMessage&, const Protocol::RpcRequest&, RpcResponseSuccess, RpcResponseFailure)> RpcHandler;
/// Callback type for RPC group completion handlers
typedef std::function<void()> RpcGroupCompletionHandler;

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
  RpcCall(RpcEngine &rpc, RpcId rpcId, const NodeIdentifier &destination, RpcCallSuccess success,
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
   * @param msg Raw routed message that contained the response
   */
  void done(const Protocol::RpcResponse &response, const RoutedMessage &msg);
  
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
  RpcCallSuccess m_success;
  /// RPC failure handler
  RpcResponseFailure m_failure;
};

UNISPHERE_SHARED_POINTER(RpcCall);

/**
 * RPC call options can be used to specify per-call options.
 */
class UNISPHERE_EXPORT RpcCallOptions {
public:
  /**
   * Constructs default options.
   */
  RpcCallOptions()
    : timeout(15)
  {}
  
  /**
   * Sets up routing options for this RPC call.
   */
  RpcCallOptions &setRoutingOptions(const RoutingOptions &opts) { routingOptions = opts; return *this; }
  
  /**
   * Sets this call's timeout in seconds.
   */
  RpcCallOptions &setTimeout(int seconds) { timeout = seconds; return *this; }
  
  /**
   * Forces the RPC call to be delivered via a specific link.
   */
  RpcCallOptions &setDeliverVia(const NodeIdentifier &linkId) { routingOptions.setDeliverVia(linkId); return *this; }
  
  /**
   * Forces the RPC call to be delivered via a specific link.
   */
  RpcCallOptions &setDeliverVia(const Contact &contact) { routingOptions.setDeliverVia(contact); return *this; }

  /**
   * Sets direct delivery requirement - this means that the local routing decision
   * will never try to handle destination identifier resolution.
   */
  RpcCallOptions &setDirectDelivery(bool delivery) { routingOptions.setDirectDelivery(delivery); return *this; }
public:
  /// Routing options
  RoutingOptions routingOptions;
  /// Timeout in seconds
  int timeout;
};

/**
 * Class for returning responses to RPC method calls.
 */
template<class ResponseType>
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
   * @param opts Routing options
   */
  RpcResponse(ResponseType rsp, const RoutingOptions &opts)
    : response(rsp),
      routingOptions(opts)
  {}
public:
  /// The actual response message
  ResponseType response;
  /// Routing options
  RoutingOptions routingOptions;
};

// Forward declaration of RpcCallGroup
class RpcCallGroup;
UNISPHERE_SHARED_POINTER(RpcCallGroup);

/**
 * This class handles RPC calls between nodes. Each RPC call is composed of
 * two parts - request and response, both formatted as Protocol Buffers
 * messages.
 */
class UNISPHERE_EXPORT RpcEngine {
public:
  /// Recent RPC call list size
  static const size_t recent_size = 20;
  
  /**
   * Class constructor.
   * 
   * @param router Router instance
   */
  explicit RpcEngine(CompactRouter &router);
  
  RpcEngine(const RpcEngine&) = delete;
  RpcEngine &operator=(const RpcEngine&) = delete;
  
  /**
   * Returns the router instance associated with this RPC engine.
   */
  inline CompactRouter &router() const { return m_router; }

  /**
   * Starts an RPC call group.
   *
   * @param complete Completion handler that gets invoked when all grouped
   *   calls are completed
   */
  RpcCallGroupPtr group(RpcGroupCompletionHandler complete);
  
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
  void call(const NodeIdentifier &destination, const std::string &method,
            const RequestType &request, std::function<void(const ResponseType&, const RoutedMessage&)> success,
            RpcResponseFailure failure = nullptr, const RpcCallOptions &opts = RpcCallOptions())
  {
    // Serialize Protocol Buffers message into the payload
    std::vector<char> buffer(request.ByteSize());
    request.SerializeToArray(&buffer[0], buffer.size());
    
    createCall(destination, method, buffer,
      [success](const Protocol::RpcResponse &rsp, const RoutedMessage &msg) { success(message_cast<ResponseType>(rsp.data()), msg); },
      failure, opts
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
  void call(const NodeIdentifier &destination, const std::string &method,
            const RequestType &request = RequestType(), const RpcCallOptions &opts = RpcCallOptions())
  {
    // Serialize Protocol Buffers message into the payload
    std::vector<char> buffer(request.ByteSize());
    request.SerializeToArray(&buffer[0], buffer.size());
    
    // Create the call and immediately cancel it as we don't need a confirmation
    RpcCallPtr call = createCall(destination, method, buffer, nullptr, nullptr, opts);
    call->cancel();
  }
  
  /**
   * Cancels a given pending RPC call.
   * 
   * @param rpcId Call's unique identifier
   */
  void cancel(RpcId rpcId);
  
  /**
   * Verifies that the specific RPC call was an actual recent outgoing call performed
   * by this node.
   */
  bool isRecentCall(RpcId rpcId);
  
  /**
   * Registers a new RPC method call.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType>
  void registerMethod(const std::string &method, std::function<RpcResponse<ResponseType>(const RequestType&, const RoutedMessage&, RpcId rpcId)> impl)
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
  void registerMethod(const std::string &method, std::function<void(const RequestType&, const RoutedMessage&, RpcId rpcId)> impl)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_methods[method] = createBasicMethodHandler<RequestType>(method, impl);
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
  void registerInterceptMethod(const std::string &method, std::function<void(const RequestType&, const RoutedMessage&, RpcId rpcId)> impl)
  {
    RecursiveUniqueLock lock(m_mutex);
    m_interceptMethods[method] = createBasicMethodHandler<RequestType>(method, impl);
  }

  /**
   * Removes an already registered method.
   *
   * @param method Method name
   */
  void unregisterMethod(const std::string &method);
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
   * @param opts Call options
   */
  RpcCallPtr createCall(const NodeIdentifier &destination, const std::string &method,
                    const std::vector<char> &payload, RpcCallSuccess success,
                    RpcResponseFailure failure, const RpcCallOptions &opts);
  
  /**
   * Creates a new RPC method handler.
   *
   * @param method Method name
   * @param impl Method implementation
   */
  template<typename RequestType, typename ResponseType>
  RpcHandler createBasicMethodHandler(const std::string &method, std::function<RpcResponse<ResponseType>(const RequestType&, const RoutedMessage&, RpcId)> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const RoutedMessage &msg, const Protocol::RpcRequest &request,
                  RpcResponseSuccess success, RpcResponseFailure failure) {
      try {
        // Deserialize the message and call method implementation
        RpcResponse<ResponseType> rsp = impl(message_cast<RequestType>(request.data()), msg, request.rpc_id());
        Protocol::RpcResponse response;
        response.set_rpc_id(request.rpc_id());
        response.set_error(false);
        
        // Serialize response message into the payload
        std::vector<char> buffer(rsp.response.ByteSize());
        rsp.response.SerializeToArray(&buffer[0], buffer.size());
        response.set_data(&buffer[0], buffer.size());
        success(response, rsp.routingOptions);
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
  RpcHandler createBasicMethodHandler(const std::string &method, std::function<void(const RequestType&, const RoutedMessage&, RpcId)> impl)
  {
    // Wrap the implementation with proper serializers/deserializers depending on
    // specified request and response types
    return [impl](const RoutedMessage &msg, const Protocol::RpcRequest &request,
                  RpcResponseSuccess success, RpcResponseFailure failure) {
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
  Protocol::RpcResponse getErrorResponse(RpcId rpcId, RpcErrorCode code,
                                         const std::string &message) const;
  
  /**
   * Sends a response message back to the originator.
   * 
   * @param msg Original request message
   * @param response Response message
   * @param opts Routing options
   */
  void respond(const RoutedMessage &msg, const Protocol::RpcResponse &response,
               const RoutingOptions &opts = RoutingOptions());
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
  CompactRouter &m_router;
  /// Mutex protecting the RPC engine
  std::recursive_mutex m_mutex;
  /// Pending RPC calls
  std::unordered_map<RpcId, RpcCallPtr> m_pendingCalls;
  /// Registered RPC methods
  std::unordered_map<std::string, RpcHandler> m_methods;
  /// Registered RPC intercept methods
  std::unordered_map<std::string, RpcHandler> m_interceptMethods;
  /// Recent RPC calls
  boost::multi_index_container<
    RpcId,
    midx::indexed_by<
      midx::sequenced<>,
      midx::hashed_unique<midx::identity<RpcId>>
    >
  > m_recentCalls;
};

class UNISPHERE_EXPORT RpcCallGroup : public boost::enable_shared_from_this<RpcCallGroup> {
public:
  friend class RpcEngine;
  
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
  void call(const NodeIdentifier &destination, const std::string &method,
            const RequestType &request, std::function<void(const ResponseType&, const RoutedMessage&)> success,
            RpcResponseFailure failure = nullptr, const RpcCallOptions &opts = RpcCallOptions())
  {
    // Call group is stored in call handler closures and will be destroyed after all
    // handlers are completed
    auto self = shared_from_this();

    m_calls++;
    m_engine.call<RequestType, ResponseType>(
      destination, method, request,
      m_strand.wrap([self, success](const ResponseType &rsp, const RoutedMessage &msg) {
        if (success)
          success(rsp, msg);
        self->checkCompletion();
      }),
      m_strand.wrap([self, failure](RpcErrorCode code, const std::string &msg) {
        if (failure)
          failure(code, msg);
        self->checkCompletion();
      }),
      opts
    );
  }

  /**
   * Starts an RPC call subgroup.
   *
   * @param complete Completion handler that gets invoked when all grouped
   *   calls are completed
   */
  RpcCallGroupPtr group(RpcGroupCompletionHandler complete);
protected:
  /**
   * Constructs an RPC call group.
   *
   * @param complete Completion handler
   */
  RpcCallGroup(RpcEngine &engine, RpcGroupCompletionHandler complete);

  /**
   * Checks whether the completion handler needs to be invoked.
   */
  void checkCompletion();
private:
  /// Reference to the RPC engine
  RpcEngine &m_engine;
  /// Completion handler
  RpcGroupCompletionHandler m_handler;
  /// Number of pending calls
  int m_calls;
  /// Strand to ensure that all handlers in a group are executed serially
  boost::asio::strand m_strand;
};

UNISPHERE_SHARED_POINTER(RpcCallGroup);
  
}

#endif
