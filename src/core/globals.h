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
#ifndef UNISPHERE_CORE_GLOBALS_H
#define UNISPHERE_CORE_GLOBALS_H

#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>

#include <mutex>

#include "hash_tuple.h"
#include "logging.h"

using boost::format;

namespace UniSphere {

/// Unique lock type
using UniqueLock = std::unique_lock<std::mutex>;
/// Recursive unique lock type
using RecursiveUniqueLock = std::unique_lock<std::recursive_mutex>;

}

// Library export macros
#define UNISPHERE_NO_EXPORT __attribute__ ((visibility("hidden")))
#define UNISPHERE_EXPORT __attribute__ ((visibility("default")))

// Helper macro for creating a shared pointer type
#define UNISPHERE_SHARED_POINTER(Class) \
  class Class; \
  using Class##Ptr = boost::shared_ptr<Class>; \
  using Class##WeakPtr = boost::weak_ptr<Class>;

// Helper macro for the pimpl idiom by the use of a d-reference
#define UNISPHERE_DECLARE_PRIVATE(Class) \
  friend class Class##Private; \
  boost::shared_ptr<class Class##Private> d;

#define UNISPHERE_DECLARE_PUBLIC(Class) \
  Class &q;

#define UNISPHERE_DECLARE_PREMATURE_PARENT_SETTER(Class) \
  struct Setup##Class##Public { \
    Setup##Class##Public(Class &parent, Class##Private *self) \
    { \
      parent.d = boost::shared_ptr<Class##Private>(self); \
    } \
  } w;

#endif
