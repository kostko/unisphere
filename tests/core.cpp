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

#include "core/context.h"

using namespace UniSphere;

TEST_CASE("core/context", "verify that context operations work")
{
  // Library initializer
  LibraryInitializer init;
  // Framework context
  Context ctx;
  // Seed the random number generator to get predictable results
  ctx.basicRng().seed(42);

  SECTION("s1", "adding jitter to timers")
  {
    int timing[] = { 937, 1149, 1226, 841, 1116, 1140 };
    for (int x : timing) {
      REQUIRE(ctx.roughly(1000).total_seconds() == x);
    }
  }

  SECTION("s2", "exponential backoff")
  {
    int backoff[] = { 0, 5, 0, 28, 46, 19, 35, 34, 55, 49, 65, 46 };
    for (int i = 0; i < 12; i++) {
      REQUIRE(ctx.backoff(i, 5, 60).total_seconds() == backoff[i]);
    }
  }
}
