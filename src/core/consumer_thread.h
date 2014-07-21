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
#ifndef UNISPHERE_CORE_CONSUMER_THREAD_H
#define UNISPHERE_CORE_CONSUMER_THREAD_H

#include "core/blocking_queue.h"

namespace UniSphere {

/**
 * A generic consumer thread.
 */
template <typename T>
class UNISPHERE_EXPORT ConsumerThread {
public:
  /**
   * Class constructor.
   */
  ConsumerThread()
    : m_running(false)
  {
  }

  /**
   * Returns true if the consumer thread is currently running.
   */
  bool isRunning() const
  {
    return m_running;
  }

  /**
   * Starts the consumer thread.
   */
  void start()
  {
    m_running = true;
    m_thread = std::move(boost::thread([this]() {
      while (m_running) {
        consume(m_queue.pop());
      }
    }));
  }

  /**
   * Signals the consumer thread to stop.
   */
  void stop()
  {
    m_running = false;
  }

  /**
   * Consumes an item from the queue.
   *
   * @param item Item to be consumed
   */
  virtual void consume(T &&item) const
  {}

  /**
   * Pushes an item into the queue.
   *
   * @param item Item to push
   */
  void push(const T &item)
  {
    m_queue.push(item);
  }
private:
  /// Thread instance
  boost::thread m_thread;
  /// Blocking queue
  BlockingQueue<T> m_queue;
  /// Running flag
  bool m_running;
};

}

#endif
