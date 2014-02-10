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
#ifndef UNISPHERE_CORE_SIGNAL_H
#define UNISPHERE_CORE_SIGNAL_H

#include "core/context.h"

#include <boost/signals2/signal.hpp>

namespace UniSphere {

namespace detail {

/**
 * Dispatcher for variadic arguments.
 */
template<typename Base, typename... TT>
struct VariadicDispatcher {
  /// Tuple containing the arguments
  std::tuple<TT...> args;
  /// Parent signal reference
  Base &parent;

  template<int ...> struct seq {};
  template<int N, int ...S> struct gens : gens<N-1, N-1, S...> {};
  template<int ...S> struct gens<0, S...>{ typedef seq<S...> type; };

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

}

/**
 * Wrapper around boost::signals2::signal to support signals that
 * can be dispatched later by the Boost.ASIO dispatcher.
 */
template<typename... T>
class DeferrableSignal : public boost::signals2::signal<T...> {
public:
  /// Base class
  typedef typename boost::signals2::signal<T...>::signal base_class;

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
    detail::VariadicDispatcher<base_class, TT...> d{std::tuple<TT...>(args...), *this};
    m_context.service().post([d]() {
      d.dispatch();
    });
  }
private:
  /// UNISPHERE context
  Context &m_context;
};

/**
 * Wrapper around boost::signals2::signal to support signals that
 * are rate-limited and delayed before being emitted.
 */
template <int Delay, int MaxDelay, int Rate, int Period = 0>
class PeriodicRateDelayedSignal : public boost::signals2::signal<void()> {
public:
  /// Base class
  typedef typename boost::signals2::signal<void()>::signal base_class;

  BOOST_STATIC_ASSERT(Delay <= MaxDelay);
  BOOST_STATIC_ASSERT(MaxDelay <= Rate);

  /**
   * Class constructor.
   */
  PeriodicRateDelayedSignal(Context &context)
    : base_class(),
      m_context(context),
      m_timer(context.service()),
      m_periodic(context.service()),
      m_limited(false)
  {
  }

  /**
   * Starts periodic signal emission.
   */
  void start()
  {
    if (Period > 0) {
      m_periodic.expires_from_now(m_context.roughly(Period));
      m_periodic.async_wait(boost::bind(&PeriodicRateDelayedSignal::periodicInvoke, this, _1));
    }
  }

  /**
   * Stops periodic signal emission.
   */
  void stop()
  {
    m_periodic.cancel();
  }

  /**
   * Call operator.
   */
  void operator()()
  {
    UniqueLock lock(m_mutex);
    auto now = boost::posix_time::microsec_clock::universal_time();

    if (m_limited)
      return;

    if (!m_lastEmit.is_not_a_date_time() && (now - m_lastEmit).total_seconds() < Rate) {
      // This call must be rate limited
      m_limited = true;
      m_timer.expires_from_now(m_context.roughly(Rate - (now - m_lastEmit).total_seconds()));
      m_timer.async_wait(boost::bind(&PeriodicRateDelayedSignal::limit, this, _1));
      return;
    }

    if (m_firstCall.is_not_a_date_time()) {
      // First call
      m_firstCall = now;
      m_timer.expires_from_now(m_context.roughly(Delay));
      m_timer.async_wait(boost::bind(&PeriodicRateDelayedSignal::emit, this, _1));
    } else {
      if ((now - m_firstCall + m_timer.expires_from_now()).total_seconds() < MaxDelay) {
        // We are still within the bounds of max delay, reschedule timer
        if (m_timer.expires_from_now(m_context.roughly(Delay)) > 0) {
          m_timer.async_wait(boost::bind(&PeriodicRateDelayedSignal::emit, this, _1));
        }
      }
    }
  }
protected:
  void periodicInvoke(const boost::system::error_code &error)
  {
    if (error)
      return;

    operator()();

    UniqueLock lock(m_mutex);
    m_periodic.expires_from_now(m_context.roughly(Period));
    m_periodic.async_wait(boost::bind(&PeriodicRateDelayedSignal::periodicInvoke, this, _1));
  }

  void limit(const boost::system::error_code &error)
  {
    if (error)
      return;

    m_limited = false;
    operator()();
  }

  void emit(const boost::system::error_code &error)
  {
    if (error)
      return;

    // Reset state of the signal and dispatch call
    {
      UniqueLock lock(m_mutex);
      m_firstCall = boost::posix_time::ptime();
      m_lastEmit = boost::posix_time::microsec_clock::universal_time();
    }

    base_class::operator()();
  }
private:
  /// UNISPHERE context
  Context &m_context;
  /// Mutex
  std::mutex m_mutex;
  /// Time of first call after last emit
  boost::posix_time::ptime m_firstCall;
  /// Time of last emit
  boost::posix_time::ptime m_lastEmit;
  /// Timer
  boost::asio::deadline_timer m_timer;
  /// Periodic timer
  boost::asio::deadline_timer m_periodic;
  /// Flag if signal is currently rate-limited
  bool m_limited;
};

template <int Delay, int MaxDelay>
using DelayedSignal = PeriodicRateDelayedSignal<Delay, MaxDelay, 0, 0>;

template <int Delay, int MaxDelay, int Rate>
using RateDelayedSignal = PeriodicRateDelayedSignal<Delay, MaxDelay, Rate, 0>;

template <int Rate>
using RateLimitedSignal = PeriodicRateDelayedSignal<0, 0, Rate, 0>;

template <int Rate, int Period>
using PeriodicRateLimitedSignal = PeriodicRateDelayedSignal<0, 0, Rate, Period>;

/**
 * An all-true slot combiner. If any slot returns false, further slots are not
 * called and false is returned as a final result.
 */
class AllTrueCombiner {
public:
  typedef bool result_type;

  template<typename InputIterator>
  result_type operator()(InputIterator first, InputIterator last)
  {
    if (first == last)
      return true;

    while (first != last) {
      if (*first == false)
        return false;
      ++first;
    }

    return true;
  }
};

}

#endif
