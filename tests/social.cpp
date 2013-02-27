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
      REQUIRE(record);
      REQUIRE(record->nodeId == nodeId);
      REQUIRE(record->type == NameRecord::Type::Authority);
    }

    // Ensure that neighbor lookup works with exact match (middle)
    std::list<NameRecordPtr> records = ndb.lookupClosest(c, NameDatabase::LookupType::ClosestNeighbors);
    REQUIRE(records.size() == 2);
    REQUIRE(records.front()->nodeId == b);
    REQUIRE(records.back()->nodeId == d);
  }
}
