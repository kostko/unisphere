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
#include "social/name_database.h"
#include "social/compact_router.h"
#include "social/routing_table.h"
#include "social/social_identity.h"
#include "social/rpc_channel.h"
#include "interplex/link_manager.h"
#include "rpc/engine.hpp"
#include "src/social/core_methods.pb.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/log/attributes/constant.hpp>

namespace midx = boost::multi_index;

namespace UniSphere {

/// NIB index tags
namespace NIBTags {
  class DestinationId;
  class TypeDestination;
  class TypeAge;
}

/// Name information base (database of all name records)
typedef boost::multi_index_container<
  NameRecordPtr,
  midx::indexed_by<
    // Index by node identifier, sorted by type
    midx::ordered_unique<
      midx::tag<NIBTags::DestinationId>,
      midx::composite_key<
        NameRecord,
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NodeIdentifier, nodeId),
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NameRecord::Type, type)
      >
    >,

    // Index by record type and destination identifier
    midx::ordered_unique<
      midx::tag<NIBTags::TypeDestination>,
      midx::composite_key<
        NameRecord,
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NameRecord::Type, type),
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NodeIdentifier, nodeId)
      >
    >,

    // Index by record type and last update timestamp
    midx::ordered_non_unique<
      midx::tag<NIBTags::TypeAge>,
      midx::composite_key<
        NameRecord,
        BOOST_MULTI_INDEX_MEMBER(NameRecord, NameRecord::Type, type),
        BOOST_MULTI_INDEX_MEMBER(NameRecord, boost::posix_time::ptime, lastUpdate)
      >
    >
  >
> NameInformationBase;

class NameDatabasePrivate {
public:
  /**
   * Class constructor.
   */
  NameDatabasePrivate(CompactRouter &router, NameDatabase &ndb);

  void store(NameRecordPtr record);

  void store(const NodeIdentifier &nodeId,
             const std::list<LandmarkAddress> &addresses,
             NameRecord::Type type);

  void remove(const NodeIdentifier &nodeId, NameRecord::Type type);

  void clear();

  const NameRecordPtr lookup(const NodeIdentifier &nodeId) const;

  std::list<NodeIdentifier> diff(const std::unordered_map<NodeIdentifier, NameRecordPtr> &source) const;

  void refreshLocalRecord();

  void fullUpdate(const NodeIdentifier &peer);

  void diffUpdate(const std::list<NodeIdentifier> &diff, const NodeIdentifier &peer) const;

  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const;
public:
  /**
   * Called when a record expires.
   */
  void entryTimerExpired(const boost::system::error_code &error, NameRecordWeakPtr record);
public:
  UNISPHERE_DECLARE_PUBLIC(NameDatabase)

  /// Router
  CompactRouter &m_router;
  /// Logger instance
  Logger m_logger;
  /// Mutex protecting the name database
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;
  /// Name database
  NameInformationBase m_nameDb;
  /// Signal for refreshing local address record
  PeriodicRateLimitedSignal<30, 600> m_refreshSignal;

  /// Statistics
  NameDatabase::Statistics m_statistics;
};

NameRecord::NameRecord(Context &context, const NodeIdentifier &nodeId, Type type)
  : nodeId(nodeId),
    type(type),
    expiryTimer(context.service()),
    timestamp(0),
    seqno(0)
{
}

LandmarkAddress NameRecord::landmarkAddress() const
{
  if (addresses.empty())
    return LandmarkAddress();

  return addresses.front();
}

boost::posix_time::seconds NameRecord::ttl() const
{
  int ttlSecs = 0;
  switch (type) {
    case Type::Cache: ttlSecs = 300; break;
    case Type::SloppyGroup: ttlSecs = 1200; break;
  }

  return boost::posix_time::seconds(ttlSecs);
}

boost::posix_time::time_duration NameRecord::age() const
{
  return boost::posix_time::microsec_clock::universal_time() - lastUpdate;
}

bool NameRecord::isMoreFresh(NameRecordPtr other) const
{
  if (timestamp > other->timestamp)
    return true;
  else if (timestamp == other->timestamp && seqno > other->seqno)
    return true;

  return false;
}

NameDatabasePrivate::NameDatabasePrivate(CompactRouter &router, NameDatabase &ndb)
  : q(ndb),
    m_router(router),
    m_logger(logging::keywords::channel = "name_db"),
    m_localId(router.identity().localId()),
    m_refreshSignal(router.context())
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(m_localId));

  m_refreshSignal.connect(boost::bind(&NameDatabasePrivate::refreshLocalRecord, this));
  m_refreshSignal.start();
}

void NameDatabasePrivate::refreshLocalRecord()
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(boost::make_tuple(m_localId, NameRecord::Type::SloppyGroup));
  if (it == m_nameDb.end())
    return;

  NameRecordPtr record = *it;
  std::uint32_t timestamp = m_router.context().getCurrentTimestamp();
  // If the record has just been updated, we don't need to refresh as this would reset seqno
  if (record->timestamp == timestamp)
    return;

  m_statistics.localRefreshes++;
  record->timestamp = timestamp;
  record->seqno = 0;
  q.signalExportRecord(record, NodeIdentifier::INVALID);
}

void NameDatabasePrivate::store(NameRecordPtr record)
{
  BOOST_ASSERT(record->type == NameRecord::Type::SloppyGroup);

  // Prevent storage of null node identifiers or null L-R addresses
  if (record->nodeId.isNull() || record->addresses.empty()) {
    m_statistics.recordDrops++;
    return;
  }

  // Ignore foreign-originated records for local node
  if (record->nodeId == m_localId) {
    m_statistics.recordDrops++;
    return;
  }

  // Set last update timestamp
  record->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  // Call hooks that can filter the record
  if (!q.signalImportRecord(record)) {
    m_statistics.recordDrops++;
    return;
  }

  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(boost::make_tuple(record->nodeId, NameRecord::Type::SloppyGroup));
  if (it == m_nameDb.end()) {
    // Insertion of a new record
    m_nameDb.insert(record);
    m_statistics.recordInsertions++;
  } else {
    // Update of an existing record
    NameRecordPtr existing = *it;
    if (!record->isMoreFresh(existing))
      return;

    BOOST_ASSERT(m_nameDb.replace(it, record));
    m_statistics.recordUpdates++;
  }

  // Install a timer on the record
  record->expiryTimer.expires_from_now(m_router.context().roughly(record->ttl()));
  record->expiryTimer.async_wait(boost::bind(&NameDatabasePrivate::entryTimerExpired, this, _1, NameRecordWeakPtr(record)));

  // Export entry to sloppy group peers
  q.signalExportRecord(record, NodeIdentifier::INVALID);
}

void NameDatabasePrivate::store(const NodeIdentifier &nodeId,
                                const std::list<LandmarkAddress> &addresses,
                                NameRecord::Type type)
{
  BOOST_ASSERT(type != NameRecord::Type::SloppyGroup || nodeId == m_localId);
  BOOST_ASSERT(type == NameRecord::Type::SloppyGroup || nodeId != m_localId);

  // Prevent storage of null node identifiers or null L-R addresses
  if (nodeId.isNull() || addresses.empty())
    return;

  RecursiveUniqueLock lock(m_mutex);

  NameRecordPtr record;
  auto it = m_nameDb.find(boost::make_tuple(nodeId, type));
  if (it == m_nameDb.end()) {
    // Insertion of a new record
    record = boost::make_shared<NameRecord>(m_router.context(), nodeId, type);
    record->timestamp = m_router.context().getCurrentTimestamp();
    record->seqno = 0;
    record->lastUpdate = boost::posix_time::microsec_clock::universal_time();
    m_nameDb.insert(record);

    // Ensure that only a limited number of cache entries is accepted
    if (type == NameRecord::Type::Cache) {
      auto &nibCache = m_nameDb.get<NIBTags::TypeAge>();
      if (nibCache.count(NameRecord::Type::Cache) >= NameDatabase::max_cache_entries) {
        nibCache.erase(nibCache.lower_bound(NameRecord::Type::Cache));
      }
    }
  } else {
    // Update of an existing record
    record = *it;

    std::uint32_t timestamp = m_router.context().getCurrentTimestamp();
    if (nodeId == m_localId) {
      // Check if we need to update seqno for local record
      if (record->timestamp == timestamp) {
        record->seqno++;
      } else {
        record->timestamp = timestamp;
        record->seqno = 0;
      }
    }

    m_nameDb.modify(it, [&](NameRecordPtr r) {
      r->lastUpdate = boost::posix_time::microsec_clock::universal_time();
    });
    record->addresses.clear();
  }

  for (const LandmarkAddress &address : addresses) {
    if (address.isNull())
      continue;

    record->addresses.push_back(address);
    if (record->addresses.size() >= NameDatabase::max_stored_addresses)
      break;
  }

  // Own records should never expire, so we don't install a timer
  if (record->nodeId != m_localId) {
    record->expiryTimer.expires_from_now(m_router.context().roughly(record->ttl()));
    record->expiryTimer.async_wait(boost::bind(&NameDatabasePrivate::entryTimerExpired, this, _1, NameRecordWeakPtr(record)));
  }

  // Local sloppy group entry should be exported to sloppy group peers
  if (nodeId == m_localId)
    m_refreshSignal();
}

void NameDatabasePrivate::remove(const NodeIdentifier &nodeId, NameRecord::Type type)
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(boost::make_tuple(nodeId, type));
  if (it != m_nameDb.end())
    m_nameDb.erase(it);
}

void NameDatabasePrivate::clear()
{
  RecursiveUniqueLock lock(m_mutex);
  m_nameDb.clear();
  m_statistics = NameDatabase::Statistics();
}

void NameDatabasePrivate::fullUpdate(const NodeIdentifier &peer)
{
  RecursiveUniqueLock lock(m_mutex);

  auto records = m_nameDb.get<NIBTags::TypeDestination>().equal_range(NameRecord::Type::SloppyGroup);
  for (auto it = records.first; it != records.second; ++it) {
    q.signalExportRecord(*it, peer);
  }
}

void NameDatabasePrivate::diffUpdate(const std::list<NodeIdentifier> &diff, const NodeIdentifier &peer) const
{
  RecursiveUniqueLock lock(m_mutex);

  for (const NodeIdentifier &nodeId : diff) {
    auto it = m_nameDb.find(nodeId);
    if (it != m_nameDb.end())
      q.signalExportRecord(*it, peer);
  }
}

const NameRecordPtr NameDatabasePrivate::lookup(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end())
    return NameRecordPtr();

  return *it;
}

std::list<NodeIdentifier> NameDatabasePrivate::diff(const std::unordered_map<NodeIdentifier, NameRecordPtr> &source) const
{
  RecursiveUniqueLock lock(m_mutex);
  std::list<NodeIdentifier> result;
  auto records = m_nameDb.get<NIBTags::TypeDestination>().equal_range(NameRecord::Type::SloppyGroup);
  for (auto it = records.first; it != records.second; ++it) {
    auto jt = source.find((*it)->nodeId);
    if (jt == source.end() || (*it)->isMoreFresh(jt->second))
      result.push_back((*it)->nodeId);
  }

  return result;
}

void NameDatabasePrivate::entryTimerExpired(const boost::system::error_code &error, NameRecordWeakPtr record)
{
  if (error)
    return;

  if (NameRecordPtr r = record.lock()) {
    // Remove the record from the name database
    m_statistics.recordExpirations++;
    remove(r->nodeId, r->type);
  }
}

void NameDatabasePrivate::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
{
  RecursiveUniqueLock lock(m_mutex);

  stream << "*** Stored name records:" << std::endl;
  for (auto rp : m_nameDb) {
    NameRecordPtr record = rp;
    stream << "  " << record->nodeId.hex() << " ";
    if (resolve)
      stream << "(" << resolve(record->nodeId) << ") t=";

    switch (record->type) {
      case NameRecord::Type::Cache: stream << "C"; break;
      case NameRecord::Type::SloppyGroup: stream << "S"; break;
      default: stream << "?"; break;
    }
    stream << " laddr=" << record->landmarkAddress() << " ";
    stream << " age=" << record->age().total_seconds() << "s";
    stream << std::endl;
  }
}

NameDatabase::NameDatabase(CompactRouter &router)
  : d(new NameDatabasePrivate(router, *this))
{
}

size_t NameDatabase::size() const
{
  return d->m_nameDb.size();
}

size_t NameDatabase::sizeActive() const
{
  RecursiveUniqueLock lock(d->m_mutex);

  auto &nibType = d->m_nameDb.get<NIBTags::TypeDestination>();
  return nibType.count(NameRecord::Type::SloppyGroup);
}

size_t NameDatabase::sizeCache() const
{
  RecursiveUniqueLock lock(d->m_mutex);

  auto &nibType = d->m_nameDb.get<NIBTags::TypeDestination>();
  return nibType.count(NameRecord::Type::Cache);
}

const NameDatabase::Statistics &NameDatabase::statistics() const
{
  return d->m_statistics;
}

std::list<NameRecordPtr> NameDatabase::getNames(NameRecord::Type type) const
{
  RecursiveUniqueLock lock(d->m_mutex);
  std::list<NameRecordPtr> names;
  
  auto records = d->m_nameDb.get<NIBTags::TypeDestination>().equal_range(type);
  for (auto it = records.first; it != records.second; ++it) {
    names.push_back(*it);
  }

  return names;
}

void NameDatabase::store(NameRecordPtr record)
{
  d->store(record);
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const std::list<LandmarkAddress> &addresses,
                         NameRecord::Type type)
{
  d->store(nodeId, addresses, type);
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const LandmarkAddress &address,
                         NameRecord::Type type)
{
  d->store(nodeId, std::list<LandmarkAddress>{ address }, type);
}

void NameDatabase::remove(const NodeIdentifier &nodeId, NameRecord::Type type)
{
  d->remove(nodeId, type);
}

void NameDatabase::clear()
{
  d->clear();
}

void NameDatabase::fullUpdate(const NodeIdentifier &peer)
{
  d->fullUpdate(peer);
}

void NameDatabase::diffUpdate(const std::list<NodeIdentifier> &diff, const NodeIdentifier &peer) const
{
  d->diffUpdate(diff, peer);
}

const NameRecordPtr NameDatabase::lookup(const NodeIdentifier &nodeId) const
{
  return d->lookup(nodeId);
}

std::list<NodeIdentifier> NameDatabase::diff(const std::unordered_map<NodeIdentifier, NameRecordPtr> &source) const
{
  return d->diff(source);
}

void NameDatabase::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
{
  d->dump(stream, resolve);
}

}
