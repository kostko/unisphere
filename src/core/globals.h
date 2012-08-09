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
#ifndef UNISPHERE_CORE_GLOBALS_H
#define UNISPHERE_CORE_GLOBALS_H

#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

using boost::format;

namespace UniSphere {

// Locking typedefs used for passing an upgradable lock around
typedef boost::shared_lock<boost::shared_mutex> SharedLock;
typedef boost::upgrade_lock<boost::shared_mutex> UpgradableLock;
typedef boost::shared_ptr<UpgradableLock> UpgradableLockPtr;
typedef boost::upgrade_to_unique_lock<boost::shared_mutex> UpgradeToUniqueLock;
typedef boost::unique_lock<boost::shared_mutex> UniqueLock;
typedef boost::unique_lock<boost::recursive_mutex> RecursiveUniqueLock;

}

// Library export macros
#define UNISPHERE_NO_EXPORT __attribute__ ((visibility("hidden")))
#define UNISPHERE_EXPORT __attribute__ ((visibility("default")))

// Helper macros for hiding implementation details
#define UNISPHERE_PRIVATE_DETAILS(Class) \
  private: \
    boost::shared_ptr<Class##Private> d; 

// Helper macro for creating a shared pointer type
#define UNISPHERE_SHARED_POINTER(Class) \
  class Class; \
  typedef boost::shared_ptr<Class> Class##Ptr; \
  typedef boost::weak_ptr<Class> Class##WeakPtr;

#endif
