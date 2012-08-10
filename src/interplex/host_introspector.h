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
#ifndef UNISPHERE_INTERPLEX_HOSTINTROSPECTOR_H
#define UNISPHERE_INTERPLEX_HOSTINTROSPECTOR_H

#include "interplex/contact.h"

namespace UniSphere {

/**
 * A class that performs introspection of local networking configuration
 * and can determine things like host's contact IP addresses.
 */
class UNISPHERE_EXPORT HostIntrospector {
public:
  /**
   * Class constructor.
   */
  HostIntrospector();
  
  /**
   * Returns local contact information.
   */
  static Contact localContact(unsigned short port);
};

}

#endif
