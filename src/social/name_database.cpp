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

#include <botan/botan.h>

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
    m_localRefreshTimer(router.context().service())
{
}

void NameDatabase::initialize()
{
  UNISPHERE_LOG(m_router.linkManager(), Info, "NameDatabase: Initializing name database.");

  // Register core name database RPC methods
  registerCoreRpcMethods();

  // Schedule local address refresh
  m_localRefreshTimer.expires_from_now(boost::posix_time::seconds(600));
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

void NameDatabase::store(const NodeIdentifier &nodeId, const std::list<LandmarkAddress> &addresses, NameRecord::Type type)
{
  // Prevent storage of null node identifiers or null L-R addresses
  if (nodeId.isNull() || addresses.empty() || addresses.size() > NameDatabase::max_stored_addresses)
    return;

  RecursiveUniqueLock lock(m_mutex);

  NameRecordPtr record;
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end()) {
    // Insertion of a new record
    record = NameRecordPtr(new NameRecord(m_router.context(), nodeId, type));
    m_nameDb.insert(record);
    // TODO: Ensure that only sqrt(n*logn) Authority entries are stored at the local node
  } else {
    // Update of an existing record
    record = *it;

    // Prevent non-cached entries from changing type into cached entries
    if (record->type != NameRecord::Type::Cache && type == NameRecord::Type::Cache)
      return;
    // Prevent authoritative records from being overwritten by other records
    if (record->type == NameRecord::Type::Authority && type != NameRecord::Type::Authority)
      return;

    m_nameDb.modify(it, [&](NameRecordPtr r) {
      r->type = type;
    });
    record->addresses.clear();
  }

  for (const LandmarkAddress &address : addresses) {
    if (address.isNull())
      continue;

    record->addresses.push_back(address);
  }
  record->lastUpdate = boost::posix_time::microsec_clock::universal_time();

  record->expiryTimer.expires_from_now(boost::posix_time::seconds(record->ttl()));
  record->expiryTimer.async_wait(boost::bind(&NameDatabase::entryTimerExpired, this, _1, record));
}

void NameDatabase::store(const NodeIdentifier &nodeId, const LandmarkAddress &address, NameRecord::Type type)
{
  store(nodeId, std::list<LandmarkAddress>{ address }, type);
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

const NameRecordPtr NameDatabase::lookup(const NodeIdentifier &nodeId) const
{
  RecursiveUniqueLock lock(m_mutex);
  auto it = m_nameDb.find(nodeId);
  if (it == m_nameDb.end())
    return NameRecordPtr();

  return *it;
}

const std::list<NameRecordPtr> NameDatabase::lookupClosest(const NodeIdentifier &nodeId,
                                                           LookupType type,
                                                           const NodeIdentifier &origin) const
{
  RecursiveUniqueLock lock(m_mutex);
  std::list<NameRecordPtr> records;

  // Limit selection to authoritative records; if no authoritative records are
  // available in the name database, return an empty list
  auto &nibType = m_nameDb.get<NIBTags::TypeDestination>();
  const auto authRecords = nibType.equal_range(NameRecord::Type::Authority);
  if (authRecords.first == authRecords.second)
    return records;

  // Find the entry with longest common prefix
  auto it = nibType.upper_bound(boost::make_tuple(NameRecord::Type::Authority, nodeId));
  if (it == authRecords.second) {
    --it;

    // If we are looking for the closest match itself, we have found it
    if (type == LookupType::Closest) {
      records.push_back(*it);
      return records;
    }
  }

  // Check if previous entry has longer common prefix
  auto pit = it;
  if (it != authRecords.first) {
    if ((*(--pit))->nodeId.longestCommonPrefix(nodeId) >
        (*it)->nodeId.longestCommonPrefix(nodeId))
      it = pit;
  }

  switch (type) {
    case LookupType::Closest: {
      records.push_back(*it);
      break;
    }

    case LookupType::ClosestNeighbors: {
      // Predecessor
      pit = it;
      if (pit == authRecords.first)
        pit = authRecords.second;

      records.push_back(*(--pit));

      // Successor
      pit = it;
      if (++pit != authRecords.second)
        records.push_back(*pit);
      else
        records.push_back(*authRecords.first);
      break;
    }

    case LookupType::ClosestNotSelf: {
      if (origin.isValid() && (*it)->nodeId == origin) {
        auto sit = it;
        if (++sit == authRecords.second)
          sit = authRecords.first;
        pit = it;
        if (pit == authRecords.first)
          pit = authRecords.second;
        --pit;

        if (sit == it)
          sit = pit;
        if (pit == it)
          pit = sit;

        // Choose the record that is closest
        if ((*sit)->nodeId.longestCommonPrefix(nodeId) >
            (*pit)->nodeId.longestCommonPrefix(nodeId))
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
  }

  return records;
}

void NameDatabase::remoteLookupClosest(const NodeIdentifier &nodeId,
                                       LookupType type,
                                       std::function<void(const std::list<NameRecordPtr>&)> success,
                                       std::function<void()> failure) const
{
  remoteLookupClosest(nodeId, type, RpcCallGroupPtr(), success, failure);
}

void NameDatabase::remoteLookupClosest(const NodeIdentifier &nodeId,
                                       LookupType type,
                                       RpcCallGroupPtr rpcGroup,
                                       std::function<void(const std::list<NameRecordPtr>&)> success,
                                       std::function<void()> failure) const
{
  RpcEngine &rpc = m_router.rpcEngine();

  auto successHandler = [success, this](const Protocol::LookupAddressResponse &response, const RoutedMessage&) {
    std::list<NameRecordPtr> records;

    for (int i = 0; i < response.records_size(); i++) {
      const Protocol::LookupAddressResponse::Record &rr = response.records(i);
      NameRecordPtr record(new NameRecord(
        m_router.context(),
        NodeIdentifier(rr.nodeid(), NodeIdentifier::Format::Raw),
        NameRecord::Type::Authority
      ));
      records.push_back(record);

      std::list<LandmarkAddress> addresses;
      for (int j = 0; j < rr.addresses_size(); j++) {
        const Protocol::LandmarkAddress &laddr = rr.addresses(j);
        record->addresses.push_back(LandmarkAddress(
          NodeIdentifier(laddr.landmarkid(), NodeIdentifier::Format::Raw),
          laddr.address()
        ));
      }
    }

    success(records);
  };

  auto failureHandler = [failure](RpcErrorCode, const std::string&) {
    if (failure)
      failure();
  };

  for (const NodeIdentifier &landmarkId : getLandmarkCaches(nodeId, true)) {
    Protocol::LookupAddressRequest request;
    request.set_nodeid(nodeId.raw());
    switch (type) {
      case LookupType::Closest: request.set_type(Protocol::LookupAddressRequest::CLOSEST); break;
      case LookupType::ClosestNeighbors: request.set_type(Protocol::LookupAddressRequest::CLOSEST_NEIGHBORS); break;
      case LookupType::ClosestNotSelf: request.set_type(Protocol::LookupAddressRequest::CLOSEST_NOT_SELF); break;
    }

    if (rpcGroup) {
      rpcGroup->call<Protocol::LookupAddressRequest, Protocol::LookupAddressResponse>(
        landmarkId, "Core.NameDb.LookupAddress", request, successHandler, failureHandler,
        RpcCallOptions().setDirectDelivery(true)
      );
    } else {
      rpc.call<Protocol::LookupAddressRequest, Protocol::LookupAddressResponse>(
        landmarkId, "Core.NameDb.LookupAddress", request, successHandler, failureHandler,
        RpcCallOptions().setDirectDelivery(true)
      );
    }
  }
}

void NameDatabase::entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record)
{
  if (error)
    return;

  remove(record->nodeId);
}

std::string NameDatabase::hashIdentifier(const NodeIdentifier &nodeId) const
{
  Botan::Pipe pipe(new Botan::Hash_Filter("MD5"));
  pipe.process_msg(nodeId.raw());
  return pipe.read_all_as_string(0);
}

void NameDatabase::registerLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Register landmark into the consistent hashing ring
  m_bucketTree[hashIdentifier(landmarkId)] = landmarkId;
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_router.identity().localId()) != m_publishLandmarks)
    publishLocalAddress();
}

void NameDatabase::unregisterLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Remove landmark from the consistent hashing ring
  m_bucketTree.erase(hashIdentifier(landmarkId));
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_router.identity().localId()) != m_publishLandmarks)
    publishLocalAddress();
}

std::unordered_set<NodeIdentifier> NameDatabase::getLandmarkCaches(const NodeIdentifier &nodeId, bool neighbors) const
{
  std::unordered_set<NodeIdentifier> landmarks;
  if (m_bucketTree.empty())
    return landmarks;

  std::string itemHash = hashIdentifier(nodeId);
  auto it = m_bucketTree.find(itemHash);
  if (it == m_bucketTree.end()) {
    it = m_bucketTree.upper_bound(itemHash);
    if (it == m_bucketTree.end())
      it = m_bucketTree.begin();
  }

  landmarks.insert(it->second);

  if (neighbors) {
    auto pit = it;

    // Include predecessor
    if (pit != m_bucketTree.begin())
      landmarks.insert((--pit)->second);
    else
      landmarks.insert((--m_bucketTree.end())->second);

    // Include successor
    pit = it;
    if (++pit != m_bucketTree.end())
      landmarks.insert(pit->second);
    else
      landmarks.insert(m_bucketTree.begin()->second);
  }
  
  return landmarks;
}

void NameDatabase::publishLocalAddress()
{
  RecursiveUniqueLock lock(m_mutex);
  RpcEngine &rpc = m_router.rpcEngine();

  // TODO: Ensure that publish requests are buffered and rate limited

  // Prepare request for publishing the local address(es)
  Protocol::PublishAddressRequest request;
  for (const LandmarkAddress &address : m_router.routingTable().getLocalAddresses(NameDatabase::max_stored_addresses)) {
    Protocol::LandmarkAddress *laddr = request.add_addresses();
    laddr->set_landmarkid(address.landmarkId().raw());
    for (Vport port : address.path())
      laddr->add_address(port);
  }

  // Determine which landmarks are responsible for our local address
  m_publishLandmarks = getLandmarkCaches(m_router.identity().localId());
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

      // Verify that this node is actually responsible for the node that wants to publish
      if (getLandmarkCaches(msg.sourceNodeId()).count(m_router.identity().localId()) == 0)
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

        case Protocol::LookupAddressRequest::CLOSEST: {
          records = lookupClosest(nodeId, LookupType::Closest);
          break;
        }

        case Protocol::LookupAddressRequest::CLOSEST_NEIGHBORS: {
          records = lookupClosest(nodeId, LookupType::ClosestNeighbors);
          break;
        }

        case Protocol::LookupAddressRequest::CLOSEST_NOT_SELF: {
          records = lookupClosest(nodeId, LookupType::ClosestNotSelf, msg.sourceNodeId());
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
  for (auto lp : m_bucketTree) {
    stream << "  " << lp.second.hex();
    if (resolve)
      stream << " (" << resolve(lp.second) << ")";

    stream << std::endl;
  }
}

}
