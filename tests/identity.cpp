/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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

using namespace UniSphere;

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
    NodeIdentifier n3("invalid");
    
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
  }
}
