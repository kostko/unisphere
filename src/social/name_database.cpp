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
#include "interplex/link_manager.h"

#include "src/social/core_methods.pb.h"

namespace UniSphere {

NameRecord::NameRecord(Context &context, const NodeIdentifier &nodeId, Type type)
  : nodeId(nodeId),
    type(type),
    expiryTimer(context.service())
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
    case Type::Authority: ttlSecs = 1200; break;
    case Type::SloppyGroup: ttlSecs = 1200; break;
  }

  return boost::posix_time::seconds(ttlSecs);
}

boost::posix_time::time_duration NameRecord::age() const
{
  return boost::posix_time::microsec_clock::universal_time() - lastUpdate;
}

NameDatabase::NameDatabase(CompactRouter &router)
  : m_router(router),
    m_localId(router.identity().localId()),
    m_localRefreshTimer(router.context().service())
{
}

void NameDatabase::initialize()
{
  RecursiveUniqueLock lock(m_mutex);

  UNISPHERE_LOG(m_router.linkManager(), Info, "NameDatabase: Initializing name database.");

  // Register core name database RPC methods
  registerCoreRpcMethods();

  // Schedule local address refresh
  m_localRefreshTimer.expires_from_now(boost::posix_time::seconds(15));
  m_localRefreshTimer.async_wait(boost::bind(&NameDatabase::refreshLocalAddress, this, _1));
}

void NameDatabase::shutdown()
{
  RecursiveUniqueLock lock(m_mutex);

  UNISPHERE_LOG(m_router.linkManager(), Warning, "NameDatabase: Shutting down name database.");

  // Unregister core name database RPC methods
  unregisterCoreRpcMethods();

  // Stop publishing our address
  m_localRefreshTimer.cancel();
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const std::list<LandmarkAddress> &addresses,
                         NameRecord::Type type,
                         const NodeIdentifier &originId)
{
  // Prevent storage of null node identifiers or null L-R addresses
  if (nodeId.isNull() || addresses.empty() || addresses.size() > NameDatabase::max_stored_addresses)
    return;

  RecursiveUniqueLock lock(m_mutex);

  NameRecordPtr record;
  bool exportNib = false;
  auto it = m_nameDb.find(boost::make_tuple(nodeId, type));
  if (it == m_nameDb.end()) {
    // Insertion of a new record
    record = NameRecordPtr(new NameRecord(m_router.context(), nodeId, type));
    record->originId = originId;
    record->lastUpdate = boost::posix_time::microsec_clock::universal_time();
    m_nameDb.insert(record);

    // Ensure that only a limited number of cache entries is accepted
    if (type == NameRecord::Type::Cache) {
      auto &nibCache = m_nameDb.get<NIBTags::TypeAge>();
      if (nibCache.count(NameRecord::Type::Cache) >= NameDatabase::max_cache_entries) {
        nibCache.erase(nibCache.lower_bound(NameRecord::Type::Cache));
      }
    }

    
    // TODO: Ensure that only sqrt(n*logn) Authority entries are stored at the local node
    exportNib = true;
  } else {
    // Update of an existing record
    record = *it;

    // If primary address has changed, we need to export
    if (record->landmarkAddress() != addresses.front())
      exportNib = true;

    m_nameDb.modify(it, [&](NameRecordPtr r) {
      r->originId = originId;
      r->lastUpdate = boost::posix_time::microsec_clock::universal_time();
    });
    record->addresses.clear();
  }

  for (const LandmarkAddress &address : addresses) {
    if (address.isNull())
      continue;

    record->addresses.push_back(address);
  }

  // Own records should never expire, so we don't install a timer
  if (record->nodeId != m_localId) {
    record->expiryTimer.expires_from_now(boost::posix_time::seconds(record->ttl()));
    record->expiryTimer.async_wait(boost::bind(&NameDatabase::entryTimerExpired, this, _1, record));
  }

  // Sloppy group entries should be exported to neighbors
  if (type == NameRecord::Type::SloppyGroup && exportNib)
    signalExportRecord(record, NodeIdentifier::INVALID);
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const LandmarkAddress &address,
                         NameRecord::Type type,
                         const NodeIdentifier &originId)
{
  store(nodeId, std::list<LandmarkAddress>{ address }, type, originId);
}

void NameDatabase::remove(const NodeIdentifier &nodeId, NameRecord::Type type)
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(boost::make_tuple(nodeId, type));
  if (it != m_nameDb.end())
    m_nameDb.erase(it);
}

void NameDatabase::clear()
{
  RecursiveUniqueLock lock(m_mutex);
  m_nameDb.clear();
}

void NameDatabase::fullUpdate(const NodeIdentifier &peer)
{
  RecursiveUniqueLock lock(m_mutex);

  auto records = m_nameDb.get<NIBTags::TypeDestination>().equal_range(NameRecord::Type::SloppyGroup);
  for (auto it = records.first; it != records.second; ++it) {
    signalExportRecord(*it, peer);
  }
}

const NameRecordPtr NameDatabase::lookup(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end())
    return NameRecordPtr();

  return *it;
}

const std::list<NameRecordPtr> NameDatabase::lookupSloppyGroup(const NodeIdentifier &nodeId,
                                                               size_t prefixLength,
                                                               const NodeIdentifier &origin,
                                                               LookupType type) const
{
  RecursiveUniqueLock lock(m_mutex);
  std::list<NameRecordPtr> records;

  // Limit selection to authoritative records within the sloppy group; if no such
  // records are available in the name database, return an empty list
  auto &nibType = m_nameDb.get<NIBTags::TypeDestination>();
  NodeIdentifier groupStart = origin.prefix(prefixLength);
  NodeIdentifier groupEnd = origin.prefix(prefixLength, 0xFF);

  // Check for invalid queries and return an empty list in this case
  if (nodeId < groupStart || nodeId > groupEnd)
    return records;

  const auto lowerLimit = nibType.lower_bound(boost::make_tuple(NameRecord::Type::Authority, groupStart));
  const auto upperLimit = nibType.upper_bound(boost::make_tuple(NameRecord::Type::Authority, groupEnd));
  if (lowerLimit == upperLimit)
    return records;

  // Find the entry with longest common prefix
  auto it = nibType.upper_bound(boost::make_tuple(NameRecord::Type::Authority, nodeId));
  if (it == upperLimit)
    --it;

  // Check if previous entry is closer
  auto pit = it;
  if (it != lowerLimit) {
    if ((*(--pit))->nodeId.distanceTo(nodeId) < (*it)->nodeId.distanceTo(nodeId))
      it = pit;
  }

  switch (type) {
    case LookupType::Closest: {
      if (origin.isValid() && (*it)->nodeId == origin) {
        auto sit = it;
        if (++sit == upperLimit)
          sit = lowerLimit;
        pit = it;
        if (pit == lowerLimit)
          pit = upperLimit;
        --pit;

        if (sit == it)
          sit = pit;
        if (pit == it)
          pit = sit;

        // Choose the record that is closest
        if ((*sit)->nodeId.distanceTo(nodeId) < (*pit)->nodeId.distanceTo(nodeId))
          it = sit;
        else
          it = pit;

        // If there are no other records available in the name database, we return
        // an empty set instead
        if ((*it)->nodeId == nodeId)
          return records;
      }

      records.push_back(*it);
      break;
    }

    case LookupType::ClosestNeighbors: {
      // Predecessor
      if ((*it)->nodeId < nodeId) {
        records.push_back(*it);
      } else {
        pit = it;
        if (pit == lowerLimit)
          pit = upperLimit;

        records.push_back(*(--pit));
      }

      // Successor
      if ((*it)->nodeId > nodeId) {
        records.push_back(*it);
      } else {
        pit = it;
        if (++pit != upperLimit)
          records.push_back(*pit);
        else
          records.push_back(*lowerLimit);
      }
      break;
    }
  }

  return records;
}

void NameDatabase::remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                                           size_t prefixLength,
                                           LookupType type,
                                           std::function<void(const std::list<NameRecordPtr>&)> complete) const
{
  remoteLookupSloppyGroup(nodeId, prefixLength, type, RpcCallGroupPtr(), complete);
}

void NameDatabase::remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                                           size_t prefixLength,
                                           LookupType type,
                                           RpcCallGroupPtr rpcGroup,
                                           std::function<void(const std::list<NameRecordPtr>&)> complete) const
{
  RpcEngine &rpc = m_router.rpcEngine();
  // A shared pointer to list of records that will be accumulated during the lookup; this
  // is needed because multiple RPC calls might be needed to fulfill this request. This
  // pointer is stored in closures so after the completion handler is invoked, the pointer
  // will also be destroyed
  boost::shared_ptr<std::list<NameRecordPtr>> records(new std::list<NameRecordPtr>);

  auto successHandler = [records, this](const Protocol::LookupAddressResponse &response, const RoutedMessage&) {
    for (int i = 0; i < response.records_size(); i++) {
      const Protocol::LookupAddressResponse::Record &rr = response.records(i);
      NameRecordPtr record(new NameRecord(
        m_router.context(),
        NodeIdentifier(rr.nodeid(), NodeIdentifier::Format::Raw),
        NameRecord::Type::Authority
      ));
      records->push_back(record);

      for (int j = 0; j < rr.addresses_size(); j++) {
        const Protocol::LandmarkAddress &laddr = rr.addresses(j);
        record->addresses.push_back(LandmarkAddress(
          NodeIdentifier(laddr.landmarkid(), NodeIdentifier::Format::Raw),
          laddr.address()
        ));
      }
    }
  };

  auto completeHandler = [complete, records]() {
    // Call the completion handler and pass it the records reference
    complete(*records);
    // After this point the records list will be destroyed
  };

  RpcCallGroupPtr subgroup = rpcGroup ? rpcGroup->group(completeHandler) : rpc.group(completeHandler);

  for (const NodeIdentifier &landmarkId : getLandmarkCaches(nodeId, prefixLength)) {
    Protocol::LookupAddressRequest request;
    request.set_nodeid(nodeId.raw());
    request.set_prefixlength(prefixLength);
    switch (type) {
      case LookupType::Closest: request.set_type(Protocol::LookupAddressRequest::SG_CLOSEST); break;
      case LookupType::ClosestNeighbors: request.set_type(Protocol::LookupAddressRequest::SG_CLOSEST_NEIGHBORS); break;
    }

    subgroup->call<Protocol::LookupAddressRequest, Protocol::LookupAddressResponse>(
      landmarkId, "Core.NameDb.LookupAddress", request, successHandler, nullptr,
      RpcCallOptions().setDirectDelivery(true)
    );
  }
}

void NameDatabase::entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record)
{
  if (error)
    return;

  remove(record->nodeId, record->type);
}

void NameDatabase::registerLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Register landmark into the consistent hashing ring
  m_bucketTree.insert(landmarkId);
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_localId) != m_publishLandmarks)
    publishLocalAddress();
}

void NameDatabase::unregisterLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Remove landmark from the consistent hashing ring
  m_bucketTree.erase(landmarkId);
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_localId) != m_publishLandmarks)
    publishLocalAddress();
}

std::unordered_set<NodeIdentifier> NameDatabase::getLandmarkCaches(const NodeIdentifier &nodeId,
                                                                   size_t sgPrefixLength) const
{
  RecursiveUniqueLock lock(m_mutex);
  assert(nodeId.isValid());

  std::unordered_set<NodeIdentifier> landmarks;
  if (m_bucketTree.empty())
    return landmarks;

  bool exact = true;
  bool wrap = false;
  auto it = m_bucketTree.find(nodeId);
  if (it == m_bucketTree.end()) {
    exact = false;
    it = m_bucketTree.upper_bound(nodeId);
    if (it == m_bucketTree.end()) {
      it = m_bucketTree.begin();
      wrap = true;
    }
  }

  landmarks.insert(*it);

  if (sgPrefixLength > 0) {
    NodeIdentifier groupStart = nodeId.prefix(sgPrefixLength);
    NodeIdentifier groupEnd = nodeId.prefix(sgPrefixLength, 0xFF);
    auto lowerLimit = m_bucketTree.lower_bound(nodeId.prefix(sgPrefixLength));
    auto upperLimit = m_bucketTree.upper_bound(groupEnd);

    // Include predecessor
    auto pit = it;
    if (pit == lowerLimit) {
      if (upperLimit == m_bucketTree.end())
        landmarks.insert(*m_bucketTree.begin());
      else
        landmarks.insert(*upperLimit);

      pit = upperLimit;
    } else if (pit == m_bucketTree.begin()) {
      pit = m_bucketTree.end();
    }

    landmarks.insert(*(--pit));

    // Include successor
    pit = it;
    if (exact) {
      if (*it == groupEnd) {
        landmarks.insert(*lowerLimit);
      } else {
        ++pit;
        if (pit == upperLimit) {
          landmarks.insert(*lowerLimit);
          if (pit == m_bucketTree.end())
            landmarks.insert(*m_bucketTree.begin());
          else
            landmarks.insert(*pit);
        } else {
          landmarks.insert(*pit);
        }
      }
    } else if (pit == upperLimit || wrap) {
      landmarks.insert(*lowerLimit);
    }
  }
  
  return landmarks;
}

void NameDatabase::publishLocalAddress()
{
  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();

  // Also update the address in local name database for announcement to the local sloppy group
  std::list<LandmarkAddress> addresses = m_router.routingTable().getLocalAddresses(NameDatabase::max_stored_addresses);
  store(m_localId, addresses, NameRecord::Type::SloppyGroup);

  // TODO: Ensure that publish requests are buffered and rate limited

  // Prepare request for publishing the local address(es)
  Protocol::PublishAddressRequest request;
  for (const LandmarkAddress &address : addresses) {
    Protocol::LandmarkAddress *laddr = request.add_addresses();
    laddr->set_landmarkid(address.landmarkId().raw());
    for (Vport port : address.path())
      laddr->add_address(port);
  }

  // Determine which landmarks are responsible for our local address
  m_publishLandmarks = getLandmarkCaches(m_localId);
  for (const NodeIdentifier &landmarkId : m_publishLandmarks) {
    // Send RPC to publish the address
    rpc.call<Protocol::PublishAddressRequest>(
      landmarkId,
      "Core.NameDb.PublishAddress",
      request,
      RpcCallOptions().setDirectDelivery(true)
    );
  }
}

void NameDatabase::refreshLocalAddress(const boost::system::error_code &error)
{
  if (error)
    return;

  publishLocalAddress();

  // Schedule local address refresh
  m_localRefreshTimer.expires_from_now(boost::posix_time::seconds(600));
  m_localRefreshTimer.async_wait(boost::bind(&NameDatabase::refreshLocalAddress, this, _1));
}

void NameDatabase::registerCoreRpcMethods()
{
  RpcEngine &rpc = m_router.rpcEngine();

  // Address publishing mechanism
  rpc.registerMethod<Protocol::PublishAddressRequest>("Core.NameDb.PublishAddress",
    [this](const Protocol::PublishAddressRequest &request, const RoutedMessage &msg, RpcId) {
      // If this node is not a landmark, ignore publish request
      if (!m_router.routingTable().isLandmark())
        return;

      // Store address in local name database
      std::list<LandmarkAddress> addresses;
      for (int i = 0; i < request.addresses_size(); i++) {
        const Protocol::LandmarkAddress &laddr = request.addresses(i);
        addresses.push_back(LandmarkAddress(
          NodeIdentifier(laddr.landmarkid(), NodeIdentifier::Format::Raw),
          laddr.address()
        ));
      }

      store(msg.sourceNodeId(), addresses, NameRecord::Type::Authority);
    }
  );

  // Exact address lookup mechanism
  rpc.registerMethod<Protocol::LookupAddressRequest, Protocol::LookupAddressResponse>("Core.NameDb.LookupAddress",
    [this](const Protocol::LookupAddressRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<Protocol::LookupAddressResponse> {
      Protocol::LookupAddressResponse response;

      // If this node is not a landmark, ignore lookup request
      if (!m_router.routingTable().isLandmark())
        throw RpcException(RpcErrorCode::BadRequest, "Not a landmark node!");

      NodeIdentifier nodeId(request.nodeid(), NodeIdentifier::Format::Raw);
      std::list<NameRecordPtr> records;
      switch (request.type()) {
        case Protocol::LookupAddressRequest::EXACT: {
          NameRecordPtr record = lookup(nodeId);
          if (record)
            records.push_back(record);
          break;
        }

        case Protocol::LookupAddressRequest::SG_CLOSEST: {
          if (!request.has_prefixlength())
            throw RpcException(RpcErrorCode::BadRequest, "Missing prefix length!");

          records = lookupSloppyGroup(nodeId, request.prefixlength(), msg.sourceNodeId(), LookupType::Closest);
          break;
        }

        case Protocol::LookupAddressRequest::SG_CLOSEST_NEIGHBORS: {
          if (!request.has_prefixlength())
            throw RpcException(RpcErrorCode::BadRequest, "Missing prefix length!");

          records = lookupSloppyGroup(nodeId, request.prefixlength(), msg.sourceNodeId(), LookupType::ClosestNeighbors);
          break;
        }

        default: {
          throw RpcException(RpcErrorCode::BadRequest, "Unsupported lookup type!");
        }
      }

      for (NameRecordPtr record : records) {
        Protocol::LookupAddressResponse::Record *rr = response.add_records();
        rr->set_nodeid(record->nodeId.raw());

        // Lookup the proper name record and include it in response
        if (record && record->type == NameRecord::Type::Authority) {
          for (const LandmarkAddress &address : record->addresses) {
            Protocol::LandmarkAddress *laddr = rr->add_addresses();
            laddr->set_landmarkid(address.landmarkId().raw());
            for (Vport port : address.path())
              laddr->add_address(port);
          }
        }
      }

      return response;
    }
  );
}

void NameDatabase::unregisterCoreRpcMethods()
{
  RpcEngine &rpc = m_router.rpcEngine();
  rpc.unregisterMethod("Core.NameDb.PublishAddress");
  rpc.unregisterMethod("Core.NameDb.LookupAddress");
}

void NameDatabase::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
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
      case NameRecord::Type::Authority: stream << "A"; break;
      case NameRecord::Type::SloppyGroup: stream << "S"; break;
      default: stream << "?"; break;
    }
    stream << " laddr=" << record->landmarkAddress() << " ";
    stream << " age=" << record->age().total_seconds() << "s";
    stream << std::endl;
  }

  stream << "*** Registred landmarks:" << std::endl;
  for (const NodeIdentifier &landmarkId : m_bucketTree) {
    stream << "  " << landmarkId.hex();
    if (resolve)
      stream << " (" << resolve(landmarkId) << ")";

    stream << std::endl;
  }
}

}
