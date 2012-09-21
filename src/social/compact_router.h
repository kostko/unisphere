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
#ifndef UNISPHERE_SOCIAL_COMPACTROUTER_H
#define UNISPHERE_SOCIAL_COMPACTROUTER_H

#include "social/social_identity.h"

namespace UniSphere {

class LinkManager;
class NetworkSizeEstimator;

class UNISPHERE_EXPORT CompactRouter {
public:
  CompactRouter(const SocialIdentity &identity, LinkManager &manager,
                NetworkSizeEstimator &sizeEstimator);
private:
  /// Local node identity
  SocialIdentity m_identity;
  /// Link manager associated with this router
  LinkManager &m_manager;
  /// Network size estimator
  NetworkSizeEstimator &m_sizeEstimator;
};
  
}

#endif
