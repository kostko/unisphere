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
#ifndef UNISPHERE_PLEXUS_BOOTSTRAP_H
#define UNISPHERE_PLEXUS_BOOTSTRAP_H

#include "interplex/contact.h"

namespace UniSphere {
  
class LinkManager;
  
/**
 * An interface for implementing overlay bootstrap methods.
 */
class UNISPHERE_EXPORT Bootstrap {
public:
  /**
   * Class constructor.
   */
  Bootstrap();
  
  /**
   * Returns the next bootstrap contact that can be used for bootstrapping
   * the overlay network.
   */
  virtual Contact getBootstrapContact() = 0;
};

/**
 * A simple bootstrap method that contains a single contact to connect
 * to. The contact is specified at construction time.
 */
class UNISPHERE_EXPORT SingleHostBootstrap : public Bootstrap {
public:
  /**
   * Class constructor.
   * 
   * @param contact Bootstrap contact
   */
  SingleHostBootstrap(const Contact &contact);
  
  /**
   * Returns the next bootstrap contact that can be used for bootstrapping
   * the overlay network.
   */
  Contact getBootstrapContact();
private:
  /// The single bootstrap contact
  Contact m_contact;
};
  
}

#endif
