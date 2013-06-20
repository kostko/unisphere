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
#ifndef UNISPHERE_TESTBED_SIMULATION_H
#define UNISPHERE_TESTBED_SIMULATION_H

#include "interplex/contact.h"

namespace UniSphere {

namespace TestBed {

/**
 * Simulation instance.
 */
class UNISPHERE_EXPORT Simulation {
public:
  Simulation(size_t globalNodeCount);

  void createNode(const std::string &name,
                  const Contact &contact,
                  const std::list<Contact> &peers);

  void run();
private:
  UNISPHERE_DECLARE_PRIVATE(Simulation)
};

UNISPHERE_SHARED_POINTER(Simulation)

}

}

#endif
