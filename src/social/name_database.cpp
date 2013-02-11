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

namespace UniSphere {

NameRecord::NameRecord(boost::asio::io_service &service, const NodeIdentifier &nodeId, Type type)
  : nodeId(nodeId),
    type(type),
    expiryTimer(service)
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
  int ttlSecs;
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

NameDatabase::NameDatabase(CompactRouter &router)
  : m_router(router)
{
}

void NameDatabase::store(const NodeIdentifier &nodeId, const LandmarkAddress &address, NameRecord::Type type)
{
  RecursiveUniqueLock lock(m_mutex);

  NameRecordPtr record;
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end()) {
    // Insertion of a new record
    record = NameRecordPtr(new NameRecord(m_router.context().service(), nodeId, type));
    m_nameDb[record->nodeId] = record;
  } else {
    // Update of an existing record
    record = it->second;
    record->addresses.clear();
  }

  record->addresses.push_back(address);
  record->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  record->expiryTimer.expires_from_now(boost::posix_time::seconds(record->ttl()));
  record->expiryTimer.async_wait(boost::bind(&NameDatabase::entryTimerExpired, this, _1, record));
}

void NameDatabase::remove(const NodeIdentifier &nodeId)
{
  RecursiveUniqueLock lock(m_mutex);
  m_nameDb.erase(nodeId);
}

void NameDatabase::clear()
{
  RecursiveUniqueLock lock(m_mutex);
  m_nameDb.clear();
}

NameRecordPtr NameDatabase::lookup(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end())
    return NameRecordPtr();

  return it->second;
}

void NameDatabase::entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record)
{
  if (error)
    return;

  remove(record->nodeId);
}

void NameDatabase::registerLandmark(const NodeIdentifier &landmarkId)
{
  // TODO: Register landmark into the consistent hashing ring
}

void NameDatabase::unregisterLandmark(const NodeIdentifier &landmarkId)
{
  // TODO: Remove landmark from the consistent hashing ring
}

std::list<NodeIdentifier> NameDatabase::getLandmarkCaches(const NodeIdentifier &nodeId) const
{
  std::list<NodeIdentifier> landmarks;
  // TODO: Implement consistent hashing over the set of landmarks
  return landmarks;
}

}
