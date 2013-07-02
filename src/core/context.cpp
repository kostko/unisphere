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
#include "core/context.h"

#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/thread/tss.hpp>
#include <unordered_map>
#include <thread>

namespace UniSphere {

class ContextPrivate {
public:
  ContextPrivate();
public:
  /// ASIO I/O service for all network operations
  boost::asio::io_service m_io;
  /// ASIO work grouping for all network operations
  boost::asio::io_service::work m_work;
  /// The thread pool when multiple threads are used
  boost::thread_group m_pool;
  /// Mutex protecting the context
  std::recursive_mutex m_mutex;
  /// Logger instance
  Logger m_logger;
  /// Cryptographically secure random number generator (per-thread)
  boost::thread_specific_ptr<Botan::AutoSeeded_RNG> m_rng;
  /// Basic random generator that should not be used for crypto ops (per-thread)
  boost::thread_specific_ptr<std::mt19937> m_basicRng;
  /// Worker thread initializer
  std::function<void()> m_threadInitializer;
};

LibraryInitializer::LibraryInitializer()
  : m_botan("thread_safe=true")
{
  logging::add_common_attributes();
  logging::core::get()->set_logging_enabled(false);
}

ContextPrivate::ContextPrivate()
  : m_work(m_io),
    m_logger(logging::keywords::channel = "context")
{
}

Context::Context()
  : d(new ContextPrivate)
{
  // Log context initialization
  BOOST_LOG(d->m_logger) << "UNISPHERE context initialized.";
}

Context::~Context()
{
}

boost::asio::io_service &Context::service()
{
  return d->m_io;
}

Botan::RandomNumberGenerator &Context::rng()
{
  Botan::AutoSeeded_RNG *rng = d->m_rng.get();
  if (!rng) {
    rng = new Botan::AutoSeeded_RNG();
    d->m_rng.reset(rng);
  }

  return *rng;
}

std::mt19937 &Context::basicRng()
{
  std::mt19937 *basicRng = d->m_basicRng.get();
  if (!basicRng) {
    basicRng = new std::mt19937();
    d->m_basicRng.reset(basicRng);

    // Seed the basic RNG from the CSRNG
    std::uint32_t seed;
    rng().randomize((Botan::byte*) &seed, sizeof(seed));
    basicRng->seed(seed);
  }

  return *basicRng;
}

void Context::setThreadInitializer(std::function<void()> initializer)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_threadInitializer = initializer;
}

void Context::schedule(int timeout, std::function<void()> operation)
{
  // The timer pointer is passed into a closure so it will be automatically removed
  // when the operation is done executing
  typedef boost::shared_ptr<boost::asio::deadline_timer> SharedTimer;
  SharedTimer timer = SharedTimer(new boost::asio::deadline_timer(d->m_io));
  timer->expires_from_now(boost::posix_time::seconds(timeout));
  timer->async_wait([timer, operation](const boost::system::error_code&) { operation(); });
}

boost::posix_time::seconds Context::roughly(int value)
{
  std::uniform_int_distribution<int> jitter(0, value / 2);
  
  if (value <= 1)
    return boost::posix_time::seconds(value);
  else
    return boost::posix_time::seconds(value * 3 / 4 + jitter(basicRng()));
}

boost::posix_time::seconds Context::roughly(boost::posix_time::seconds value)
{
  return roughly(value.total_seconds());
}

boost::posix_time::seconds Context::backoff(size_t attempts, int interval, int maximum)
{
  if (attempts > 31)
    attempts = 31;

  std::uniform_int_distribution<unsigned int> factor(0, (1 << attempts) - 1);
  unsigned int v = factor(basicRng()) * interval;
  if (v > maximum)
    v = maximum;

  return roughly(v);
}

void Context::run(size_t threads)
{
  {
    RecursiveUniqueLock lock(d->m_mutex);
    
    // Create as many threads as specified and let them run the I/O service
    for (int i = 0; i < threads; i++) {
      d->m_pool.create_thread([this]() {
        // Perform per-thread initialization
        {
          RecursiveUniqueLock lock(d->m_mutex);
          if (d->m_threadInitializer)
            d->m_threadInitializer();
        }

        // Run the ASIO service
        d->m_io.run();

        // Destroy all per-thread RNGs
        d->m_rng.reset();
        d->m_basicRng.reset();
      });
    }
  }
  
  for (;;) {
    try {
      // Wait for the I/O threads to finish execution
      d->m_pool.join_all();
      break;
    } catch (boost::thread_interrupted &e) {
      // Context thread has been interrupted, we invoke the interruption handler
      // and then continue with waiting for the thread group to finish; this enables
      // the context thread to be reused for executing certain operations instead
      // of simply waiting for other threads
      signalInterrupted();
      continue;
    }
  }
}

void Context::stop()
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_io.stop();
}

void Context::reset()
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_io.reset();
}

}
