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
#ifndef UNISPHERE_RPC_CALLGROUP_H
#define UNISPHERE_RPC_CALLGROUP_H

#include "rpc/call.hpp"
#include "rpc/channel.hpp"
#include "rpc/options.hpp"

#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

namespace UniSphere {

/// Callback type for RPC group completion handlers
typedef std::function<void()> RpcGroupCompletionHandler;

template<typename Channel>
class RpcCallGroup;

template <typename Channel>
using RpcCallGroupPtr = boost::shared_ptr<RpcCallGroup<Channel>>;

template <typename Channel>
using RpcCallGroupWeakPtr = boost::weak_ptr<RpcCallGroup<Channel>>;

/**
 * Call groups enable handling of multiple RPC requests.
 */
template <typename Channel>
class UNISPHERE_EXPORT RpcCallGroup : public boost::enable_shared_from_this<RpcCallGroup<Channel>> {
public:
  friend class RpcEngine<Channel>;

  /**
   * Invoke all queued calls.
   */
  void start()
  {
    for (auto &call : m_queue) {
      call();
    }
    m_queue.clear();
  }

  /**
   * Queues a calls to a remote procedure.
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
            std::function<void(const ResponseType&, const typename Channel::message_type &msg)> success,
            RpcResponseFailure failure = nullptr,
            const RpcCallOptions<Channel> &opts = RpcCallOptions<Channel>())
  {
    // Call group is stored in call handler closures and will be destroyed after all
    // handlers are completed
    auto self = this->shared_from_this();

    m_calls++;
    m_queue.push_back([=]() {
      m_engine.call<RequestType, ResponseType>(
        destination,
        method,
        request,
        m_strand.wrap([self, success](const ResponseType &rsp, const typename Channel::message_type &msg) {
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
    });
  }

  /**
   * Starts an RPC call subgroup.
   *
   * @param complete Completion handler that gets invoked when all grouped
   *   calls are completed
   */
  RpcCallGroupPtr<Channel> group(RpcGroupCompletionHandler complete)
  {
    RpcCallGroupPtr<Channel> self = this->shared_from_this();

    m_calls++;
    // Can't use make_shared here as the constructor is protected
    auto group = RpcCallGroupPtr<Channel>(new RpcCallGroup<Channel>(
      m_engine,
      m_strand.wrap([self, complete]() {
        if (complete)
          complete();
        self->checkCompletion();
      })
    ));
    m_queue.push_back([group]() { group->start(); });

    return group;
  }
protected:
  /**
   * Constructs an RPC call group.
   *
   * @param complete Completion handler
   */
  RpcCallGroup(RpcEngine<Channel> &engine, RpcGroupCompletionHandler complete)
    : m_engine(engine),
      m_handler(complete),
      m_calls(0),
      m_strand(engine.context().service())
  {
  }

  /**
   * Checks whether the completion handler needs to be invoked.
   */
  void checkCompletion()
  {
    // Invoke handler when all calls have completed
    if (--m_calls <= 0 && m_handler)
      m_handler();
  }
private:
  /// Reference to the RPC engine
  RpcEngine<Channel> &m_engine;
  /// Completion handler
  RpcGroupCompletionHandler m_handler;
  /// Queued calls
  std::list<std::function<void()>> m_queue;
  /// Number of pending calls
  int m_calls;
  /// Strand to ensure that all handlers in a group are executed serially
  boost::asio::strand m_strand;
};

}

#endif
