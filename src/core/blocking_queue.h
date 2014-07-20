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
#ifndef UNISPHERE_CORE_BLOCKING_QUEUE_H
#define UNISPHERE_CORE_BLOCKING_QUEUE_H

#include "core/globals.h"

#include <mutex>
#include <condition_variable>
#include <deque>

namespace UniSphere {

/**
 * A simple implementation of a synchronized queue.
 */
template <typename T>
class UNISPHERE_EXPORT BlockingQueue {
public:
  /**
   * Pushes an item to the front of the queue and notifies any waiting
   * threads.
   *
   * @param value Item to push to the front of the queue
   */
  void push(const T &value)
  {
    {
      UniqueLock lock(m_mutex);
      m_queue.push_front(value);
    }

    m_condition.notify_one();
  }

  /**
   * Pops an item from the back of the queue. If there is nothing in
   * the queue, this method will block.
   */
  T pop()
  {
    UniqueLock lock(m_mutex);
    m_condition.wait(lock, [this] { return !m_queue.empty(); });

    T result(std::move(m_queue.back()));
    m_queue.pop_back();
    return result;
  }
private:
  /// Mutex protecting the queue
  std::mutex m_mutex;
  /// Wait condition on empty queue
  std::condition_variable m_condition;
  /// The queue
  std::deque<T> m_queue;
};

}

#endif
