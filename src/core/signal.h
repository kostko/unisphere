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
#ifndef UNISPHERE_CORE_SIGNAL_H
#define UNISPHERE_CORE_SIGNAL_H

#include <boost/signals2/signal.hpp>

namespace UniSphere {

/**
 * Wrapper around boost::signals2::signal to support signals that
 * can be dispatched later by the Boost.ASIO dispatcher.
 */
template<typename... T>
class DeferrableSignal : public boost::signals2::signal<T...> {
public:
  /// Base class
  typedef typename boost::signals2::signal<T...>::signal base_class;


  template<int ...> struct seq {};
  template<int N, int ...S> struct gens : gens<N-1, N-1, S...> {};
  template<int ...S> struct gens<0, S...>{ typedef seq<S...> type; };

  /**
   * Dispatcher for variadic arguments.
   */
  template<typename... TT>
  struct VariadicDispatcher {
    /// Tuple containing the arguments
    std::tuple<TT...> args;
    /// Parent signal reference
    base_class &parent;

    /**
     * Dispatches the stored call.
     */
    void dispatch() const
    {
      call(typename gens<sizeof...(TT)>::type());
    }

    /**
     * Helper method for variadic argument unpacking.
     */
    template<int... S>
    void call(seq<S...>) const
    {
      parent(std::get<S>(args)...);
    }
  };

  /**
   * Class constructor.
   *
   * @param context UNISPHERE context
   */
  DeferrableSignal(Context &context)
    : base_class(),
      m_context(context)
  {}

  /**
   * Performs a deferred call of this signal. Signal invocation is
   * deferred via ASIO dispatcher and is executed as soon as possible.
   */
  template<typename... TT>
  void defer(TT... args)
  {
    VariadicDispatcher<TT...> d{std::tuple<TT...>(args...), *this};
    m_context.service().post([d]() {
      d.dispatch();
    });
  }
private:
  /// UNISPHERE context
  Context &m_context;
};

}

#endif
