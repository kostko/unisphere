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

namespace UniSphere {

LibraryInitializer::LibraryInitializer()
  : m_botan("thread_safe=true")
{
}

Context::Context()
  : m_work(m_io)
{
  // Seed the basic random generator from the cryptographic random number generator
  std::uint32_t seed;
  m_rng.randomize((Botan::byte*) &seed, sizeof(seed));
  m_basicRng.seed(seed);

  // Log context initialization
  UNISPHERE_CLOG(*this, Info, "UNISPHERE Context initialized.");
}

Context::~Context()
{
}

void Context::schedule(int timeout, std::function<void()> operation)
{
  // The timer pointer is passed into a closure so it will be automatically removed
  // when the operation is done executing
  typedef boost::shared_ptr<boost::asio::deadline_timer> SharedTimer;
  SharedTimer timer = SharedTimer(new boost::asio::deadline_timer(m_io));
  timer->expires_from_now(boost::posix_time::seconds(timeout));
  timer->async_wait([timer, operation](const boost::system::error_code&) { operation(); });
}

void Context::run(size_t threads)
{
  // Create as many threads as specified and let them run the I/O service
  for (int i = 0; i < threads; i++) {
    m_pool.create_thread(boost::bind(&boost::asio::io_service::run, &m_io));
  }
  
  m_pool.join_all();
}

void Context::stop()
{
  m_io.stop();
}

}
