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
#ifndef UNISPHERE_TESTBED_SLAVE_H
#define UNISPHERE_TESTBED_SLAVE_H

#include "testbed/cluster/node.h"

namespace UniSphere {

namespace TestBed {

class UNISPHERE_EXPORT Slave : public ClusterNode {
public:
  Slave(const NodeIdentifier &nodeId,
        const std::string &ip,
        unsigned short port,
        const std::string &masterIp,
        unsigned short masterPort);

  Slave(const Slave&) = delete;
  Slave &operator=(const Slave&) = delete;
protected:
  void initialize();
private:
  UNISPHERE_DECLARE_PRIVATE(Slave)
};

}

}

#endif
