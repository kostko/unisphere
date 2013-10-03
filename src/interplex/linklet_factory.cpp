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
#include "interplex/linklet_factory.h"
#include "interplex/link_manager.h"
#include "interplex/ip_linklet.h"
#include "interplex/local_linklet.h"

namespace UniSphere {

LinkletFactory::LinkletFactory(LinkManager &manager)
  : m_manager(manager)
{
}

LinkletPtr LinkletFactory::create(const Address& address) const
{
  LinkletPtr linklet;
  switch (address.type()) {
    // IPv4/v6 connection
    case Address::Type::IP: linklet = boost::make_shared<IPLinklet>(m_manager); break;
    // Local IPC connection
    case Address::Type::Local: linklet = boost::make_shared<LocalLinklet>(m_manager); break;
    // This should not happen
    default: BOOST_ASSERT(false);
  }
  
  return linklet;
}
  
}
