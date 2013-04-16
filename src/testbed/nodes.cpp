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
#include "testbed/nodes.h"
#include "core/context.h"
#include "interplex/link_manager.h"
#include "social/compact_router.h"
#include "social/social_identity.h"

namespace UniSphere {
  
namespace TestBed {

VirtualNode::VirtualNode(Context &context,
                         NetworkSizeEstimator &sizeEstimator,
                         const std::string &name,
                         const NodeIdentifier &nodeId,
                         const std::string &ip,
                         unsigned short port)
  : name(name),
    nodeId(nodeId),
    identity(new SocialIdentity(nodeId)),
    linkManager(new LinkManager(context, nodeId)),
    router(new CompactRouter(*identity, *linkManager, sizeEstimator))
{
  linkManager->setLocalAddress(Address(ip, 0));
  linkManager->listen(Address(ip, port));
}

VirtualNode::~VirtualNode()
{
  delete router;
  delete linkManager;
  delete identity;
}

void VirtualNode::initialize()
{
  router->initialize();
}

void VirtualNode::shutdown()
{
  router->shutdown();
}

}

}
