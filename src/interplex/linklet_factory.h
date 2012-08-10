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
#ifndef UNISPHERE_INTERPLEX_LINKLETFACTORY_H
#define UNISPHERE_INTERPLEX_LINKLETFACTORY_H

#include "core/globals.h"
#include "interplex/contact.h"

namespace UniSphere {

class LinkManager;
UNISPHERE_SHARED_POINTER(Linklet)

/**
 * A linklet factory is used to create new linklets.
 */
class UNISPHERE_NO_EXPORT LinkletFactory {
public:
  /**
   * Class constructor.
   * 
   * @param manager Link manager instance
   */
  LinkletFactory(LinkManager &manager);
  
  /**
   * Creates a new linklet suitable for handling the given address.
   * 
   * @param address Address
   * @return A new linklet
   */
  LinkletPtr create(const Address &address) const;
private:
  /// Link manager associated with this linklet factory
  LinkManager &m_manager;
};
  
}

#endif
