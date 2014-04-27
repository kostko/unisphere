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
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "interplex/link_manager.h"
#include "social/compact_router.h"
#include "social/size_estimator.h"
#include "social/name_database.h"
#include "social/social_identity.h"
#include "social/peer.h"

#include <random>

using namespace UniSphere;

TEST_CASE("social/peer", "verify that peer operations work")
{
  // Library initializer
  LibraryInitializer init;
  // Framework context
  Context ctx;

  // Generate new private peer key
  PrivatePeerKey key;
  key.generate();
  // Generate contact information
  Contact contact(key);

  // Create new peer
  Peer peer(key.publicKey(), contact);

  // Test whether peer SAs work
  PrivateSignKey skey;
  skey.generate();
  auto pubSa = peer.addPeerSecurityAssociation(PeerSecurityAssociation{
    skey.publicKey(),
    boost::posix_time::minutes(5)
  });
  auto selectSa = peer.selectPeerSecurityAssociation(ctx);
  REQUIRE(selectSa == pubSa);
  peer.removePeerSecurityAssociation(skey.raw());
  selectSa = peer.selectPeerSecurityAssociation(ctx);
  REQUIRE(!selectSa);

  // Test whether private SAs work
  auto privSa = peer.createPrivateSecurityAssociation(boost::posix_time::minutes(5));
  auto checkSa = peer.getPrivateSecurityAssociation(privSa->raw());
  REQUIRE(privSa == checkSa);
}

TEST_CASE("social", "verify that compact routing operations work")
{
  // Library initializer
  LibraryInitializer init;
  // Framework context
  Context ctx;
  // Private key
  PrivatePeerKey privateKey;
  privateKey.generate();
  // Social identity
  SocialIdentity identity(privateKey);
  // Network size estimator
  OracleNetworkSizeEstimator sizeEstimator(14);
  // Link manager
  LinkManager linkManager(ctx, privateKey);
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

      ndb.store(a, laddr, NameRecord::Type::Cache);
      ndb.store(b, laddr, NameRecord::Type::Cache);
      ndb.store(c, laddr, NameRecord::Type::Cache);
      ndb.store(d, laddr, NameRecord::Type::Cache);

      // Ensure that exact lookup works
      for (NodeIdentifier nodeId : { a, b, c, d }) {
        NameRecordPtr record = ndb.lookup(nodeId);
        REQUIRE(record.get() != 0);
        REQUIRE(record->nodeId == nodeId);
        REQUIRE(record->type == NameRecord::Type::Cache);
      }
    }
  }
}
