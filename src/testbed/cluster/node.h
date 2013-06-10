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
#ifndef UNISPHERE_TESTBED_CLUSTERNODE_H
#define UNISPHERE_TESTBED_CLUSTERNODE_H

#include "core/globals.h"

namespace UniSphere {

class NodeIdentifier;
class Context;
class LinkManager;

namespace TestBed {

class UNISPHERE_EXPORT ClusterNode {
public:
  ClusterNode(const NodeIdentifier &nodeId,
              const std::string &ip,
              unsigned short port);

  ClusterNode(const ClusterNode&) = delete;
  ClusterNode &operator=(const ClusterNode&) = delete;

  void start();
protected:
  virtual void initialize() = 0;
protected:
  Context &context();

  LinkManager &linkManager();
private:
  UNISPHERE_DECLARE_PRIVATE(ClusterNode)
};

}

}

#endif
