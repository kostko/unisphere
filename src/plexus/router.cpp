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
#include "plexus/router.h"
#include "interplex/link_manager.h"

namespace UniSphere {

Router::Router(LinkManager &manager)
  : m_manager(manager),
    m_routes(manager.getLocalNodeId(), Router::bucket_size, Router::sibling_neighbourhood)
{
}

void Router::route(const RoutedMessage &msg)
{
  if (!msg.isValid()) {
    UNISPHERE_LOG(m_manager.context(), Warning, "Router: Dropping invalid message.");
    return;
  }
  
  // TODO
}

void Router::route(uint32_t sourceCompId, const NodeIdentifier &key,
                   uint32_t destinationCompId, uint32_t type,
                   const google::protobuf::Message &msg)
{
  // First encapsulate the specified application message into a routed message
  RoutedMessage rmsg(m_manager.getLocalNodeId(), sourceCompId, key, destinationCompId, type, msg);
  // Attempt to route the generated message
  route(rmsg);
}


}
