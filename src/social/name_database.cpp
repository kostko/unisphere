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
#include <boost/log/sources/severity_channel_logger.hpp>

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
    midx::ordered_unique<
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
  NameDatabasePrivate(CompactRouter &router, NameDatabase *ndb);

  void initialize();

  void shutdown();

  void store(const NodeIdentifier &nodeId,
             const std::list<LandmarkAddress> &addresses,
             NameRecord::Type type,
             const NodeIdentifier &originId = NodeIdentifier::INVALID);

  void remove(const NodeIdentifier &nodeId, NameRecord::Type type);

  void clear();

  const NameRecordPtr lookup(const NodeIdentifier &nodeId) const;

  const std::list<NameRecordPtr> lookupSloppyGroup(const NodeIdentifier &nodeId,
                                                   size_t prefixLength,
                                                   const NodeIdentifier &origin,
                                                   NameDatabase::LookupType type) const;

  void remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                               size_t prefixLength,
                               NameDatabase::LookupType type,
                               RpcCallGroupPtr<SocialRpcChannel> rpcGroup,
                               std::function<void(const std::list<NameRecordPtr>&)> complete) const;

  void fullUpdate(const NodeIdentifier &peer);

  void registerLandmark(const NodeIdentifier &landmarkId);

  void unregisterLandmark(const NodeIdentifier &landmarkId);

  std::unordered_set<NodeIdentifier> getLandmarkCaches(const NodeIdentifier &nodeId,
                                                       size_t sgPrefixLength = 0) const;

  void publishLocalAddress();

  void dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const;
public:
  /**
   * Called when a record expires.
   */
  void entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record);

  /**
   * Periodically refreshes the local address.
   */
  void refreshLocalAddress(const boost::system::error_code &error);

  /**
   * Performs registration of core RPC methods that are required for name database
   * management.
   */
  void registerCoreRpcMethods();

  /**
   * Performs unregistration of core RPC methods that are required for name database
   * management.
   */
  void unregisterCoreRpcMethods();
public:
  /// Public name database interface
  NameDatabase *q;
  /// Router
  CompactRouter &m_router;
  /// Logger instance
  logging::sources::severity_channel_logger<> m_logger;
  /// Mutex protecting the name database
  mutable std::recursive_mutex m_mutex;
  /// Local node identifier (cached from social identity)
  NodeIdentifier m_localId;
  /// Name database
  NameInformationBase m_nameDb;
  /// Bucket tree for consistent hashing
  std::set<NodeIdentifier> m_bucketTree;
  /// Landmarks that we have previously published into
  std::unordered_set<NodeIdentifier> m_publishLandmarks;
  /// Timer for periodic local address refresh
  boost::asio::deadline_timer m_localRefreshTimer;
};

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

NameDatabasePrivate::NameDatabasePrivate(CompactRouter &router, NameDatabase *ndb)
  : q(ndb),
    m_router(router),
    m_logger(logging::keywords::channel = "name_db"),
    m_localId(router.identity().localId()),
    m_localRefreshTimer(router.context().service())
{
  m_logger.add_attribute("LocalNodeID", logging::attributes::constant<NodeIdentifier>(m_localId));
}

void NameDatabasePrivate::initialize()
{
  RecursiveUniqueLock lock(m_mutex);

  BOOST_LOG_SEV(m_logger, normal) << "Initializing name database.";

  // Register core name database RPC methods
  registerCoreRpcMethods();

  // Schedule local address refresh
  m_localRefreshTimer.expires_from_now(m_router.context().roughly(15));
  m_localRefreshTimer.async_wait(boost::bind(&NameDatabasePrivate::refreshLocalAddress, this, _1));
}

void NameDatabasePrivate::shutdown()
{
  RecursiveUniqueLock lock(m_mutex);

  BOOST_LOG_SEV(m_logger, normal) << "Shutting down name database.";

  // Unregister core name database RPC methods
  unregisterCoreRpcMethods();

  // Stop publishing our address
  m_localRefreshTimer.cancel();
}

void NameDatabasePrivate::store(const NodeIdentifier &nodeId,
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
    record->expiryTimer.expires_from_now(m_router.context().roughly(record->ttl()));
    record->expiryTimer.async_wait(boost::bind(&NameDatabasePrivate::entryTimerExpired, this, _1, record));
  }

  // Sloppy group entries should be exported to neighbors
  if (type == NameRecord::Type::SloppyGroup && exportNib)
    q->signalExportRecord(record, NodeIdentifier::INVALID);
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
}

void NameDatabasePrivate::fullUpdate(const NodeIdentifier &peer)
{
  RecursiveUniqueLock lock(m_mutex);

  auto records = m_nameDb.get<NIBTags::TypeDestination>().equal_range(NameRecord::Type::SloppyGroup);
  for (auto it = records.first; it != records.second; ++it) {
    q->signalExportRecord(*it, peer);
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

const std::list<NameRecordPtr> NameDatabasePrivate::lookupSloppyGroup(const NodeIdentifier &nodeId,
                                                                      size_t prefixLength,
                                                                      const NodeIdentifier &origin,
                                                                      NameDatabase::LookupType type) const
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
    case NameDatabase::LookupType::Closest: {
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

    case NameDatabase::LookupType::ClosestNeighbors: {
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

void NameDatabasePrivate::remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                                                  size_t prefixLength,
                                                  NameDatabase::LookupType type,
                                                  RpcCallGroupPtr<SocialRpcChannel> rpcGroup,
                                                  std::function<void(const std::list<NameRecordPtr>&)> complete) const
{
  RpcEngine<SocialRpcChannel> &rpc = m_router.rpcEngine();
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

  RpcCallGroupPtr<SocialRpcChannel> subgroup = rpcGroup ? rpcGroup->group(completeHandler) : rpc.group(completeHandler);

  for (const NodeIdentifier &landmarkId : getLandmarkCaches(nodeId, prefixLength)) {
    Protocol::LookupAddressRequest request;
    request.set_nodeid(nodeId.raw());
    request.set_prefixlength(prefixLength);
    switch (type) {
      case NameDatabase::LookupType::Closest: request.set_type(Protocol::LookupAddressRequest::SG_CLOSEST); break;
      case NameDatabase::LookupType::ClosestNeighbors: request.set_type(Protocol::LookupAddressRequest::SG_CLOSEST_NEIGHBORS); break;
    }

    subgroup->call<Protocol::LookupAddressRequest, Protocol::LookupAddressResponse>(
      landmarkId, "Core.NameDb.LookupAddress", request, successHandler, nullptr,
      rpc.options().setChannelOptions(RoutingOptions().setDirectDelivery(true))
    );
  }
}

void NameDatabasePrivate::entryTimerExpired(const boost::system::error_code &error, NameRecordPtr record)
{
  if (error)
    return;

  remove(record->nodeId, record->type);
}

void NameDatabasePrivate::registerLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Register landmark into the consistent hashing ring
  m_bucketTree.insert(landmarkId);
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_localId) != m_publishLandmarks)
    publishLocalAddress();
}

void NameDatabasePrivate::unregisterLandmark(const NodeIdentifier &landmarkId)
{
  RecursiveUniqueLock lock(m_mutex);

  // Remove landmark from the consistent hashing ring
  m_bucketTree.erase(landmarkId);
  // TODO: Multiple replicas

  // Check if local address needs to be republished
  if (getLandmarkCaches(m_localId) != m_publishLandmarks)
    publishLocalAddress();
}

std::unordered_set<NodeIdentifier> NameDatabasePrivate::getLandmarkCaches(const NodeIdentifier &nodeId,
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
    NodeIdentifier groupEnd = nodeId.prefix(sgPrefixLength, 0xFF);
    auto lowerLimit = m_bucketTree.lower_bound(nodeId.prefix(sgPrefixLength));
    auto upperLimit = m_bucketTree.upper_bound(groupEnd);

    if (lowerLimit == upperLimit) {
      // No other landmarks have entries for this sloppy group, so we don't need
      // any successors or predecessors
      return landmarks;
    }

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

void NameDatabasePrivate::publishLocalAddress()
{
  RecursiveUniqueLock lock(m_mutex);
  RpcEngine<SocialRpcChannel> &rpc = m_router.rpcEngine();

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
      rpc.options().setChannelOptions(RoutingOptions().setDirectDelivery(true))
    );
  }
}

void NameDatabasePrivate::refreshLocalAddress(const boost::system::error_code &error)
{
  if (error)
    return;

  publishLocalAddress();

  // Schedule local address refresh
  m_localRefreshTimer.expires_from_now(m_router.context().roughly(600));
  m_localRefreshTimer.async_wait(boost::bind(&NameDatabasePrivate::refreshLocalAddress, this, _1));
}

void NameDatabasePrivate::registerCoreRpcMethods()
{
  RpcEngine<SocialRpcChannel> &rpc = m_router.rpcEngine();

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
    [this](const Protocol::LookupAddressRequest &request, const RoutedMessage &msg, RpcId) -> RpcResponse<SocialRpcChannel, Protocol::LookupAddressResponse> {
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

          records = lookupSloppyGroup(nodeId, request.prefixlength(), msg.sourceNodeId(), NameDatabase::LookupType::Closest);
          break;
        }

        case Protocol::LookupAddressRequest::SG_CLOSEST_NEIGHBORS: {
          if (!request.has_prefixlength())
            throw RpcException(RpcErrorCode::BadRequest, "Missing prefix length!");

          records = lookupSloppyGroup(nodeId, request.prefixlength(), msg.sourceNodeId(), NameDatabase::LookupType::ClosestNeighbors);
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

void NameDatabasePrivate::unregisterCoreRpcMethods()
{
  RpcEngine<SocialRpcChannel> &rpc = m_router.rpcEngine();
  rpc.unregisterMethod("Core.NameDb.PublishAddress");
  rpc.unregisterMethod("Core.NameDb.LookupAddress");
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

NameDatabase::NameDatabase(CompactRouter &router)
  : d(new NameDatabasePrivate(router, this))
{
}

void NameDatabase::initialize()
{
  d->initialize();
}

void NameDatabase::shutdown()
{
  d->shutdown();
}

size_t NameDatabase::size() const
{
  return d->m_nameDb.size();
}

size_t NameDatabase::sizeActive() const
{
  RecursiveUniqueLock lock(d->m_mutex);

  auto &nibType = d->m_nameDb.get<NIBTags::TypeDestination>();
  return nibType.count(NameRecord::Type::Authority) +
         nibType.count(NameRecord::Type::SloppyGroup);
}

size_t NameDatabase::sizeCache() const
{
  RecursiveUniqueLock lock(d->m_mutex);

  auto &nibType = d->m_nameDb.get<NIBTags::TypeDestination>();
  return nibType.count(NameRecord::Type::Cache);
}

NameRecordRange NameDatabase::names() const
{
  return d->m_nameDb;
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const std::list<LandmarkAddress> &addresses,
                         NameRecord::Type type,
                         const NodeIdentifier &originId)
{
  d->store(nodeId, addresses, type, originId);
}

void NameDatabase::store(const NodeIdentifier &nodeId,
                         const LandmarkAddress &address,
                         NameRecord::Type type,
                         const NodeIdentifier &originId)
{
  d->store(nodeId, std::list<LandmarkAddress>{ address }, type, originId);
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

const NameRecordPtr NameDatabase::lookup(const NodeIdentifier &nodeId) const
{
  return d->lookup(nodeId);
}

const std::list<NameRecordPtr> NameDatabase::lookupSloppyGroup(const NodeIdentifier &nodeId,
                                                               size_t prefixLength,
                                                               const NodeIdentifier &origin,
                                                               LookupType type) const
{
  return d->lookupSloppyGroup(nodeId, prefixLength, origin, type);
}

void NameDatabase::remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                                           size_t prefixLength,
                                           LookupType type,
                                           std::function<void(const std::list<NameRecordPtr>&)> complete) const
{
  d->remoteLookupSloppyGroup(nodeId, prefixLength, type, RpcCallGroupPtr<SocialRpcChannel>(), complete);
}

void NameDatabase::remoteLookupSloppyGroup(const NodeIdentifier &nodeId,
                                           size_t prefixLength,
                                           LookupType type,
                                           RpcCallGroupPtr<SocialRpcChannel> rpcGroup,
                                           std::function<void(const std::list<NameRecordPtr>&)> complete) const
{
  d->remoteLookupSloppyGroup(nodeId, prefixLength, type, rpcGroup, complete);
}

void NameDatabase::registerLandmark(const NodeIdentifier &landmarkId)
{
  d->registerLandmark(landmarkId);
}

void NameDatabase::unregisterLandmark(const NodeIdentifier &landmarkId)
{
  d->unregisterLandmark(landmarkId);
}

std::unordered_set<NodeIdentifier> NameDatabase::getLandmarkCaches(const NodeIdentifier &nodeId,
                                                                   size_t sgPrefixLength) const
{
  return d->getLandmarkCaches(nodeId, sgPrefixLength);
}

void NameDatabase::publishLocalAddress()
{
  d->publishLocalAddress();
}

void NameDatabase::dump(std::ostream &stream, std::function<std::string(const NodeIdentifier&)> resolve) const
{
  d->dump(stream, resolve);
}

}
