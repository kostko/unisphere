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
#ifndef UNISPHERE_CORE_CONTEXT_H
#define UNISPHERE_CORE_CONTEXT_H

#include <botan/botan.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <random>

#include "core/globals.h"
#include "core/logger.h"

namespace UniSphere {

/**
 * Library initializer class, must be created before any other class
 * from the UNISPHERE library can be used and must be created only
 * once!
 */
class UNISPHERE_EXPORT LibraryInitializer {
public:
  /**
   * Class constructor.
   */
  LibraryInitializer();
  
  LibraryInitializer(const LibraryInitializer&) = delete;
  LibraryInitializer &operator=(const LibraryInitializer&) = delete;
private:
  /// Botan cryptographic library context
  Botan::LibraryInitializer m_botan;
};

/**
 * UNISPHERE framework entry point.
 */
class UNISPHERE_EXPORT Context {
public:
  /**
   * Constructs a UNISPHERE context.
   */
  Context();
  
  Context(const Context&) = delete;
  Context &operator=(const Context&) = delete;
  
  /**
   * Class destructor.
   */
  ~Context();
  
  /**
   * Schedules an operation to be executed after a timeout.
   * 
   * @param timeout Number of seconds to wait before executing
   * @param operation Operation to be executed
   */
  void schedule(int timeout, std::function<void()> operation);

  /**
   * Returns a value in seconds with added random jitter.
   *
   * @param value Number of seconds
   * @return Rough number of seconds
   */
  boost::posix_time::seconds roughly(int value);

  /**
   * Returns a value in seconds with added random jitter.
   *
   * @param value Number of seconds
   * @return Rough number of seconds
   */
  boost::posix_time::seconds roughly(boost::posix_time::seconds value);

  /**
   * Computes the wait interval before next retry using the exponential
   * backoff algorithm.
   *
   * @param attempts Number of retry attempts so far
   * @param interval Base retry interval
   * @param maximum Maximum length of the interval
   * @return Number of seconds to wait before next retry
   */
  boost::posix_time::seconds backoff(size_t attempts, int interval, int maximum);
  
  /**
   * Returns the ASIO I/O service for this UNISPHERE context. This service
   * may be used for all I/O operations by context-dependent APIs.
   */
  boost::asio::io_service &service();
  
  /**
   * Returns the debug logger.
   */
  Logger &logger();

  /**
   * Returns the debug logger.
   *
   * @param component Component name
   * @param level Log level
   */
  Logger &logger(const std::string &component, Logger::Level level);

  /**
   * Returns the cryptographically secure random number generator.
   */
  Botan::RandomNumberGenerator &rng();

  /**
   * Returns a basic random number generator that should NOT be used for any
   * cryptographic operations.
   */
  std::mt19937 &basicRng();

  /**
   * Sets a specific seed for the basic random number generator. This should
   * only be used when testing.
   *
   * @param seed Seed for the basic random number generator
   */
  void setBasicRngSeed(std::uint32_t seed);
  
  /**
   * Enters the main event loop. Passing a thread pool size of greater than
   * one will use multiple threads for UNISPHERE processing.
   *
   * @param threads Size of the thread pool
   */
  void run(size_t threads = 1);
  
  /**
   * Stop the event loop interrupting all operations.
   */
  void stop();
private:
  UNISPHERE_DECLARE_PRIVATE(Context)
};

}

#endif
