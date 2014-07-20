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
#include "testbed/dataset/processor.h"
#include "core/blocking_queue.h"

#include <boost/thread.hpp>

namespace UniSphere {

namespace TestBed {

/**
 * Structure for storing queued insertion operations.
 */
struct QueuedInsertionOp {
  std::string ns;
  mongo::BSONObj bson;
};

/**
 * Structure for storing namespace commit status.
 */
class NamespaceCommitStatus {
public:
  void increment()
  {
    m_pending++;
  }

  void decrement()
  {
    if (--m_pending <= 0)
      m_condition.notify_all();
  }

  void wait()
  {
    UniqueLock lock(m_mutex);
    m_condition.wait(lock, [this] { return m_pending <= 0; });
  }
private:
  /// Mutex protecting the queue
  std::mutex m_mutex;
  /// Wait condition on empty queue
  std::condition_variable m_condition;
  /// Number of pendings records
  std::atomic<int> m_pending;
};

class DataSetProcessorPrivate {
public:
  DataSetProcessorPrivate(DataSetStorage &dss);
public:
  /// Logger
  Logger m_logger;
  /// Dataset storage reference
  DataSetStorage &m_dss;
  /// Thread
  boost::thread m_thread;
  /// Dataset record insertion queue
  BlockingQueue<QueuedInsertionOp> m_insertQueue;
  /// Mutex
  std::mutex m_mutex;
  /// Namespace status
  std::unordered_map<std::string, NamespaceCommitStatus> m_nsStatus;
  /// Running flag
  bool m_running;
};

DataSetProcessorPrivate::DataSetProcessorPrivate(DataSetStorage &dss)
  : m_logger(logging::keywords::channel = "dataset_processor"),
    m_dss(dss)
{
}

DataSetProcessor::DataSetProcessor(DataSetStorage &dss)
  : d(new DataSetProcessorPrivate(dss))
{
}

void DataSetProcessor::initialize()
{
  d->m_running = true;
  d->m_thread = std::move(boost::thread([this]() {
    mongo::ScopedDbConnection db(d->m_dss.getConnectionString());

    while (d->m_running) {
      QueuedInsertionOp op = d->m_insertQueue.pop();

      for (int retries = 0; retries < 3; retries++) {
        try {
          db->insert(op.ns, op.bson, 0, &mongo::WriteConcern::acknowledged);
          break;
        } catch (mongo::UserException &e) {
          BOOST_LOG_SEV(d->m_logger, log::error) << "Insert to dataset failed due to user exception: " << e.what();
        } catch (mongo::OperationException &e) {
          BOOST_LOG_SEV(d->m_logger, log::error) << "Insert to dataset failed due to operation exception: " << e.what();
        }
      }

      {
        UniqueLock lock(d->m_mutex);
        d->m_nsStatus[op.ns].decrement();
      }
    }

    db.done();
  }));

  BOOST_LOG(d->m_logger) << "Dataset processor initialized.";
}

void DataSetProcessor::insert(const std::string &ns, const mongo::BSONObj &bson)
{
  UniqueLock lock(d->m_mutex);
  d->m_insertQueue.push(QueuedInsertionOp{ ns, bson });
  d->m_nsStatus[ns].increment();
}

void DataSetProcessor::wait(const std::string &ns)
{
  {
    UniqueLock lock(d->m_mutex);
    if (d->m_nsStatus.find(ns) == d->m_nsStatus.end())
      return;
  }

  d->m_nsStatus[ns].wait();

  {
    UniqueLock lock(d->m_mutex);
    d->m_nsStatus.erase(ns);
  }
}

}

}
