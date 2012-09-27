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
#include "social/compact_router.h"
#include "social/size_estimator.h"
#include "interplex/link_manager.h"
#include "src/social/messages.pb.h"

namespace UniSphere {

CompactRouter::CompactRouter(const SocialIdentity &identity, LinkManager &manager,
                             NetworkSizeEstimator &sizeEstimator)
  : m_identity(identity),
    m_manager(manager),
    m_sizeEstimator(sizeEstimator),
    m_routes(sizeEstimator),
    m_announceTimer(manager.context().service())
{
  BOOST_ASSERT(identity.localId() == manager.getLocalNodeId());

  manager.signalMessageReceived.connect(boost::bind(&CompactRouter::messageReceived, this, _1));
  sizeEstimator.signalSizeChanged.connect(boost::bind(&CompactRouter::networkSizeEstimateChanged, this, _1));
}

void CompactRouter::initialize()
{
  // Compute whether we should become a landmark or not
  networkSizeEstimateChanged(m_sizeEstimator.getNetworkSize());

  // Announce ourselves to all neighbours, this will cause the addition into their
  // routing tables
  announceOurselves(boost::system::error_code());
}

void CompactRouter::announceOurselves(const boost::system::error_code &error)
{
  if (error)
    return;

  // TODO Announce ourselves to all neighbours
  Protocol::PathAnnounce announce;
  announce.set_destinationid(m_identity.localId().as(NodeIdentifier::Format::Raw));
  if (m_routes.isLandmark())
    announce.set_type(Protocol::PathAnnounce::LANDMARK);
  else
    announce.set_type(Protocol::PathAnnounce::VICINITY);

  //announce.add_reversepath(neighbour.vport)

  // Redo announce after 60 seconds
  m_announceTimer.expires_from_now(boost::posix_time::seconds(60));
  m_announceTimer.async_wait(boost::bind(&CompactRouter::announceOurselves, this, _1));
}

void CompactRouter::messageReceived(const Message &msg)
{
}

void CompactRouter::networkSizeEstimateChanged(std::uint64_t size)
{
  // Re-evaluate network size and check if we should alter our landmark status
  double x = std::generate_canonical<double, 32>(m_manager.context().basicRng());
  double n = static_cast<double>(size);
  if (x < std::sqrt(n * std::log(n)))
    m_routes.setLandmark(true);
}
  
}
