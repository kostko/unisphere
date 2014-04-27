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

#include "identity/node_identifier.h"
#include "identity/sign_key.h"
#include "identity/peer_key.h"
#include "identity/exceptions.h"

using namespace UniSphere;

TEST_CASE("identity/sign_key", "verify that sign key works")
{
  LibraryInitializer init;

  SECTION("s1", "random key")
  {
    PrivateSignKey key;
    // Generate a new private key
    key.generate();
    REQUIRE(!key.isNull());

    // Sign a message with this key
    std::string origMsg("Hello World!");
    std::string signedMessage = key.sign(origMsg);
    std::string msg = key.signOpen(signedMessage);
    REQUIRE(msg == origMsg);
  }

  SECTION("s2", "invalid decode")
  {
    KeyData invalidPrivateKey((unsigned char*) "foo", 3);
    PrivateSignKey keyA("not-a-valid-key", invalidPrivateKey, SignKey::Format::Raw);
    PrivateSignKey keyB("not-a-valid-key", invalidPrivateKey, SignKey::Format::Base64);

    // Check that keys are null
    REQUIRE(keyA.isNull());
    REQUIRE(keyB.isNull());

    // Check that sign methods throw
    REQUIRE_THROWS_AS(keyA.sign("foo"), NullKey);
    REQUIRE_THROWS_AS(keyA.signOpen("foo"), NullKey);
  }

  SECTION("s3", "specific key")
  {
    PrivateSignKey key(
      "H2wZSxxqitirRMKrQRDv2uC8e1z3F2SlELYbwcgtsdM=",
      KeyData(
        (unsigned char*) "W3qqkUybqur79JJbxIiWYcayXgt+tiWF6D+5T7/HS8YfbBlLHGqK2KtEwqtBEO/a4Lx7XPcXZKUQthvByC2x0w==",
        88
      ),
      SignKey::Format::Base64
    );

    // Check that keys have been parsed
    REQUIRE(!key.isNull());

    // Sign a message with this key
    std::string origMsg("Hello World!");
    std::string signedMessage = key.sign(origMsg);
    std::string msg = key.signOpen(signedMessage);
    REQUIRE(msg == origMsg);

    // Import/export of key
    std::ostringstream bufferOut;
    bufferOut << key;
    std::string serialized = bufferOut.str();
    std::istringstream bufferIn(serialized);
    PrivateSignKey deserialized;
    bufferIn >> deserialized;

    REQUIRE(!deserialized.isNull());
    REQUIRE(deserialized == key);
  }
}

TEST_CASE("identity/peer_key", "verify that peer key works")
{
  LibraryInitializer init;

  SECTION("s1", "random key")
  {
    PrivatePeerKey key;
    // Generate a new private key
    key.generate();
    REQUIRE(!key.isNull());

    // Check that node identifier is correctly derived
    REQUIRE(key.nodeId().isValid());
  }

  SECTION("s2", "invalid decode")
  {
    KeyData invalidPrivateKey((unsigned char*) "foo", 3);
    PrivatePeerKey keyA("not-a-valid-key", invalidPrivateKey, PeerKey::Format::Raw);
    PrivatePeerKey keyB("not-a-valid-key", invalidPrivateKey, PeerKey::Format::Base64);

    // Check that keys are null
    REQUIRE(keyA.isNull());
    REQUIRE(keyB.isNull());
  }

  SECTION("s3", "specific key")
  {
    PrivatePeerKey key(
      "wNc7fX5Kn7NgRQM9ba7x4tLFoY9A1JSfNCa5QPAK61w=",
      KeyData(
        (unsigned char*) "eiIjfOybATHLE22Ee5WZBjg9emUcG778jj4DXD5OhDs=",
        44
      ),
      PeerKey::Format::Base64
    );

    // Check that keys have been parsed
    REQUIRE(!key.isNull());

    // Check that node identifier is correctly derived
    REQUIRE(key.nodeId().isValid());
    REQUIRE(key.nodeId().hex() == "1c87b9b333cad9f491b86d89b4973c92c13826e0");

    // Import/export of key
    std::ostringstream bufferOut;
    bufferOut << key;
    std::string serialized = bufferOut.str();
    std::istringstream bufferIn(serialized);
    PrivatePeerKey deserialized;
    bufferIn >> deserialized;

    REQUIRE(!deserialized.isNull());
    REQUIRE(deserialized == key);
  }
}

TEST_CASE("identity/identifiers", "verify that node identifiers work")
{
  NodeIdentifier n1;

  SECTION("s1", "null identifier")
  {
    REQUIRE(n1.isNull());
    REQUIRE(!n1.isValid());

    NodeIdentifier n2;

    SECTION("s1.1", "comparing null identifiers")
    {
      REQUIRE(n1 == n2);
    }
  }

  SECTION("s2", "invalid identifier")
  {
    NodeIdentifier n3("invalid", NodeIdentifier::Format::Hex);

    REQUIRE(n3.isNull());
    REQUIRE(!n3.isValid());
  }

  SECTION("s3", "proper decoding of hex and binary identifiers")
  {
    NodeIdentifier n4("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n5("\x83\xd4!\x17\x88v/\xfc~\xdc\x1e" "9\x18yx\xdbI3D&", NodeIdentifier::Format::Raw);

    REQUIRE(!n4.isNull());
    REQUIRE(n4.isValid());
    REQUIRE(!n5.isNull());
    REQUIRE(n5.isValid());
    REQUIRE(n4 == n5);

    SECTION("s3.1", "proper encoding of hex identifiers")
    {
      REQUIRE(n5.as(NodeIdentifier::Format::Hex) == "83d4211788762ffc7edc1e39187978db49334426");
    }
  }

  SECTION("s4", "longest common prefix with same identifiers")
  {
    NodeIdentifier n4("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n5("\x83\xd4!\x17\x88v/\xfc~\xdc\x1e" "9\x18yx\xdbI3D&", NodeIdentifier::Format::Raw);

    REQUIRE(n4.longestCommonPrefix(n5) == n5.longestCommonPrefix(n4));
    REQUIRE(n4.longestCommonPrefix(n5) == 160);
  }

  SECTION("s5", "longest common prefix with different identifiers")
  {
    NodeIdentifier n6("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n7("83d42117898a5f29ee4016b53f915a85c7321fd2", NodeIdentifier::Format::Hex);

    REQUIRE(n6.longestCommonPrefix(n7) == n7.longestCommonPrefix(n6));
    REQUIRE(n6.longestCommonPrefix(n7) == 39);
  }

  SECTION("s6", "xor operator")
  {
    NodeIdentifier n8("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n9("83d42117898a5f29ee4016b53f915a85c7321fd2", NodeIdentifier::Format::Hex);

    REQUIRE((n8 ^ n9).as(NodeIdentifier::Format::Hex) == "0000000001fc70d5909c088c27e8225e8e015bf4");
  }

  SECTION("s7", "increment operator")
  {
    NodeIdentifier n8("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n9("0000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    n8 += 1337.0;
    n9 += 1337.0;

    REQUIRE(n8.hex() == "83d4211788762ffc7edc1e39187978db4933495f");
    REQUIRE(n9.isValid());
    REQUIRE(n9.hex() == "0000000000000000000000000000000000000539");
  }

  SECTION("s8", "distance operator")
  {
    NodeIdentifier zero("0000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    NodeIdentifier n8("83d4211788762ffc7edc1e39187978db49334426", NodeIdentifier::Format::Hex);
    NodeIdentifier n9("83d4211788762ffc7edc1e39187978dc27e10315", NodeIdentifier::Format::Hex);

    REQUIRE(n8.distanceTo(n8) == zero);
    REQUIRE(n8.distanceTo(n9).hex() == "00000000000000000000000000000000deadbeef");
    REQUIRE(n8.distanceTo(n9) == n9.distanceTo(n8));

    REQUIRE(n8.distanceToAsDouble(n8) == 0.0);
    REQUIRE(n8.distanceToAsDouble(n9) == 3735928559.0);
    REQUIRE(n8.distanceToAsDouble(n9) == n9.distanceToAsDouble(n8));
  }

  SECTION("s9", "binary conversion")
  {
    NodeIdentifier zero("0000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    NodeIdentifier n10("8000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    NodeIdentifier n11("f000000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    NodeIdentifier n12("e100000000000000000000000000000000000000", NodeIdentifier::Format::Hex);
    NodeIdentifier n13("83d4211788762ffc7edc1e39187978dc27e10315", NodeIdentifier::Format::Hex);

    REQUIRE(zero.bin().substr(0, 4) == "0000");
    REQUIRE(n10.bin().substr(0, 4)  == "1000");
    REQUIRE(n11.bin().substr(0, 4)  == "1111");
    REQUIRE(n11.bin().substr(0, 10) == "1111000000");
    REQUIRE(n12.bin().substr(0, 20) == "11100001000000000000");
    REQUIRE(n13.bin().substr(0, 30) == "100000111101010000100001000101");
  }
}
