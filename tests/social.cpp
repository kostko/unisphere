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
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "interplex/link_manager.h"
#include "social/compact_router.h"

#include <random>

using namespace UniSphere;

TEST_CASE("social", "verify that compact routing operations work")
{
  // Library initializer
  LibraryInitializer init;
  // Framework context
  Context ctx;
  // Local node identifier
  NodeIdentifier localId("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
  // Social identity
  SocialIdentity identity(localId);
  // Network size estimator
  OracleNetworkSizeEstimator sizeEstimator(14);
  // Link manager
  LinkManager linkManager(ctx, localId);
  // Router
  CompactRouter router(identity, linkManager, sizeEstimator);

  SECTION("name_database", "test name database operations")
  {
    NameDatabase &ndb = router.nameDb();

    // Test landmark identifier
    LandmarkAddress laddr(NodeIdentifier("eca9fb177f2d168dc5fcddb73691938ab0e89db1", NodeIdentifier::Format::Hex));

    SECTION("scenario_a", "first test scenario")
    {
      // Store some records into the name database
      NodeIdentifier a("230eabb94013ba3829671cf6e12164c28b22d7e3", NodeIdentifier::Format::Hex);
      NodeIdentifier b("5cc2eac8a2cd43599ad7338751c8e4c8380d3400", NodeIdentifier::Format::Hex);
      NodeIdentifier c("94fb38f98cae98b08b977a30a00238872aebaf1b", NodeIdentifier::Format::Hex);
      NodeIdentifier d("b535d22982da1ebbbb4b299a9f9a3d9dc14d60e9", NodeIdentifier::Format::Hex);

      ndb.store(a, laddr, NameRecord::Type::Authority);
      ndb.store(b, laddr, NameRecord::Type::Authority);
      ndb.store(c, laddr, NameRecord::Type::Authority);
      ndb.store(d, laddr, NameRecord::Type::Authority);

      // Ensure that exact lookup works
      for (NodeIdentifier nodeId : { a, b, c, d }) {
        NameRecordPtr record = ndb.lookup(nodeId);
        REQUIRE(record.get() != 0);
        REQUIRE(record->nodeId == nodeId);
        REQUIRE(record->type == NameRecord::Type::Authority);
      }

      // Ensure that neighbor lookup works
      auto records = ndb.lookupSloppyGroup(localId, 1, localId, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == d);
      REQUIRE(records.back()->nodeId == c);

      // Ensure that neighbor lookup works the same when entry exists
      ndb.store(localId, laddr, NameRecord::Type::Authority);
      records = ndb.lookupSloppyGroup(localId, 1, localId, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == d);
      REQUIRE(records.back()->nodeId == c);

      // Ensure that neighbor lookup works for middle node
      records = ndb.lookupSloppyGroup(c, 1, localId, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == localId);
      REQUIRE(records.back()->nodeId == d);

      // Ensure that neighbor lookup works for last node
      records = ndb.lookupSloppyGroup(d, 1, localId, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == c);
      REQUIRE(records.back()->nodeId == localId);

      // Ensure that bad lookup returns an empty list
      records = ndb.lookupSloppyGroup(a, 1, localId, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 0);

      // Ensure that lookup works the same in lower sloppy group (corner case of only two nodes in group)
      records = ndb.lookupSloppyGroup(a, 1, a, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == records.back()->nodeId);
      REQUIRE(records.front()->nodeId == b);

      // Ensure that lookup works the same in lower sloppy group
      NodeIdentifier e("3535d22982da1ebbbb4b299a9f9a3d9dc14d60e9", NodeIdentifier::Format::Hex);
      records = ndb.lookupSloppyGroup(e, 1, a, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == a);
      REQUIRE(records.back()->nodeId == b);
    }

    SECTION("scenario_b", "second test scenario")
    {
      // Store some records into the name database
      NodeIdentifier a("8513903f79586ecfe04603368e7423efdafe48c9", NodeIdentifier::Format::Hex);
      NodeIdentifier b("b9ec9c51a440776070dbf3bb2472a587a9bfd08d", NodeIdentifier::Format::Hex);
      NodeIdentifier c("cec6f71a4a86a3ec8779930ec4063199be436e61", NodeIdentifier::Format::Hex);
      NodeIdentifier d("cfafff33e390161394b12fea9181abc867e14235", NodeIdentifier::Format::Hex);

      ndb.store(a, laddr, NameRecord::Type::Authority);
      ndb.store(b, laddr, NameRecord::Type::Authority);
      ndb.store(c, laddr, NameRecord::Type::Authority);
      ndb.store(d, laddr, NameRecord::Type::Authority);

      // Ensure that lookup works correctly
      auto records = ndb.lookupSloppyGroup(c, 1, c, NameDatabase::LookupType::ClosestNeighbors);
      REQUIRE(records.size() == 2);
      REQUIRE(records.front()->nodeId == b);
      REQUIRE(records.back()->nodeId == d);
    }

    SECTION("consistent_hash", "checks whether consistent hashing works")
    {
      // Register some landmarks
      NodeIdentifier a("5650f763df8923b03e9fcfce0fb91b7a2abb8b9c", NodeIdentifier::Format::Hex);
      NodeIdentifier b("6179fc10d942e76f4d7a58a85fe4e545071cd33f", NodeIdentifier::Format::Hex);
      NodeIdentifier c("8e779b775cef0ca98dfe4e8bb7415260ad405540", NodeIdentifier::Format::Hex);
      NodeIdentifier d("f5d41adaa284ad6042ad77c7fb13aa51e39a39c2", NodeIdentifier::Format::Hex);

      ndb.registerLandmark(a);
      ndb.registerLandmark(b);
      ndb.registerLandmark(c);
      ndb.registerLandmark(d);

      // Ensure that we get the right cache locations
      auto caches = ndb.getLandmarkCaches(b, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);
      REQUIRE(caches.count(c) == 1);

      caches = ndb.getLandmarkCaches(a, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);
      REQUIRE(caches.count(c) == 1);

      caches = ndb.getLandmarkCaches(c, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(c) == 1);
      REQUIRE(caches.count(d) == 1);

      // Test intermediate values
      NodeIdentifier u("02c93f1c5c484275fc28ec7a602102d22c042979", NodeIdentifier::Format::Hex);
      caches = ndb.getLandmarkCaches(u, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);
      REQUIRE(caches.count(c) == 1);

      // Test intermediate values (global wrap around)
      NodeIdentifier v("ffc93f1c5c484275fc28ec7a602102d22c042979", NodeIdentifier::Format::Hex);
      caches = ndb.getLandmarkCaches(v, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(d) == 1);
      REQUIRE(caches.count(c) == 1);

      // Test intermediate values (local wrap around)
      NodeIdentifier w("7163e3648216535849e1321738a86ceee030c0a1", NodeIdentifier::Format::Hex);
      caches = ndb.getLandmarkCaches(w, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(c) == 1);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);

      // Add another landmark and do further tests
      NodeIdentifier e("73aa91ad8594f5f80429555cc71ad6ebac739dab", NodeIdentifier::Format::Hex);
      ndb.registerLandmark(e);

      caches = ndb.getLandmarkCaches(b, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);
      REQUIRE(caches.count(e) == 1);

      caches = ndb.getLandmarkCaches(a, 1);
      REQUIRE(caches.size() == 4);
      REQUIRE(caches.count(c) == 1);
      REQUIRE(caches.count(e) == 1);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(b) == 1);

      // Corner case with landmark at the edge of the sloppy group
      NodeIdentifier f("7fffffffffffffffffffffffffffffffffffffff", NodeIdentifier::Format::Hex);
      ndb.registerLandmark(f);

      caches = ndb.getLandmarkCaches(f, 1);
      REQUIRE(caches.size() == 3);
      REQUIRE(caches.count(e) == 1);
      REQUIRE(caches.count(f) == 1);
      REQUIRE(caches.count(a) == 1);

      // Corner case with landmark at the front of the sloppy group
      NodeIdentifier g("0000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
      ndb.registerLandmark(g);
      caches = ndb.getLandmarkCaches(g, 1);
      REQUIRE(caches.size() == 4);
      REQUIRE(caches.count(f) == 1);
      REQUIRE(caches.count(g) == 1);
      REQUIRE(caches.count(a) == 1);
      REQUIRE(caches.count(c) == 1);
    }
  }
}
