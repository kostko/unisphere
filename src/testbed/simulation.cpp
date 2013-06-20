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
#include "testbed/simulation.h"
#include "testbed/nodes.h"
#include "core/context.h"
#include "social/size_estimator.h"
#include "social/social_identity.h"

namespace UniSphere {

namespace TestBed {

class SimulationPrivate {
public:
  SimulationPrivate(size_t globalNodeCount);
public:
  /// Simulation context
  Context m_context;
  /// Size estimator
  OracleNetworkSizeEstimator m_sizeEstimator;
  /// Virtual nodes
  VirtualNodeMap m_nodes;
};

SimulationPrivate::SimulationPrivate(size_t globalNodeCount)
  : m_sizeEstimator(globalNodeCount)
{
}

Simulation::Simulation(size_t globalNodeCount)
  : d(new SimulationPrivate(globalNodeCount))
{
}

void Simulation::createNode(const std::string &name,
                            const Contact &contact,
                            const std::list<Contact> &peers)
{
  VirtualNodePtr node = VirtualNodePtr(new VirtualNode(d->m_context, d->m_sizeEstimator, name, contact));
  for (const Contact &peer : peers) {
    node->identity->addPeer(peer);
  }

  d->m_nodes.insert({{ contact.nodeId(), node }});
}

void Simulation::run()
{
}

}

}
