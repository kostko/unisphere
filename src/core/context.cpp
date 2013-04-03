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

#include <unordered_map>
#include <thread>

namespace UniSphere {

class ContextPrivate {
public:
  ContextPrivate();

  /**
   * Creates the random number generators for the local thread.
   */
  void createThreadRNGs();
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
  std::unordered_map<std::thread::id, Botan::AutoSeeded_RNG*> m_rng;
  /// Basic random generator that should not be used for crypto ops (per-thread)
  std::unordered_map<std::thread::id, std::mt19937*> m_basicRng;
  /// Seed for the basic RNG (if set to 0, a random seed is used)
  std::uint32_t m_basicRngSeed;
};

LibraryInitializer::LibraryInitializer()
  : m_botan("thread_safe=true")
{
}

ContextPrivate::ContextPrivate()
  : m_work(m_io),
    m_basicRngSeed(0)
{
}

void ContextPrivate::createThreadRNGs()
{
  RecursiveUniqueLock lock(m_mutex);
  std::thread::id tid = std::this_thread::get_id();
  Botan::AutoSeeded_RNG *rng = new Botan::AutoSeeded_RNG();
  std::mt19937 *basicRng = new std::mt19937();

  // Seed the basic random generator from the cryptographic random number generator
  if (m_basicRngSeed == 0) {
    std::uint32_t seed;
    rng->randomize((Botan::byte*) &seed, sizeof(seed));
    basicRng->seed(seed);
  } else {
    basicRng->seed(m_basicRngSeed);
  }

  // Register per-thread RNGs
  m_rng.insert({{ tid, rng }});
  m_basicRng.insert({{ tid, basicRng }});
}

Context::Context()
  : d(new ContextPrivate)
{
  // Create RNGs for the main thread
  d->createThreadRNGs();

  // Log context initialization
  UNISPHERE_CLOG(*this, Info, "UNISPHERE Context initialized.");
}

Context::~Context()
{
}

boost::asio::io_service &Context::service()
{
  return d->m_io;
}

Logger &Context::logger()
{
  return d->m_logger;
}

Botan::RandomNumberGenerator &Context::rng()
{
  RecursiveUniqueLock lock(d->m_mutex);
  return *d->m_rng.at(std::this_thread::get_id());
}

std::mt19937 &Context::basicRng()
{
  RecursiveUniqueLock lock(d->m_mutex);
  return *d->m_basicRng.at(std::this_thread::get_id());
}

void Context::setBasicRngSeed(std::uint32_t seed)
{
  RecursiveUniqueLock lock(d->m_mutex);
  d->m_basicRngSeed = seed;

  // Re-seed the existing basic RNGs (if any)
  if (seed > 0) {
    for (auto &p : d->m_basicRng)
      p.second->seed(seed);
  }
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

void Context::run(size_t threads)
{
  // Reset the I/O service when needed
  if (d->m_io.stopped())
    d->m_io.reset();
  
  // Create as many threads as specified and let them run the I/O service
  for (int i = 0; i < threads; i++) {
    d->m_pool.create_thread([this]() {
      // Initialize the random number generators
      d->createThreadRNGs();

      // Run the ASIO service
      d->m_io.run();
    });
  }
  
  d->m_pool.join_all();

  // Destroy all per-thread RNGs after threads have joined
  for (auto p : d->m_rng)
    delete p.second;
  for (auto p : d->m_basicRng)
    delete p.second;

  d->m_rng.clear();
  d->m_basicRng.clear();
}

void Context::stop()
{
  d->m_io.stop();
}

}
