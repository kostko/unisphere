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
#ifndef UNISPHERE_SOCIAL_SLOPPYGROUP_H
#define UNISPHERE_SOCIAL_SLOPPYGROUP_H

#include "core/globals.h"
#include "identity/node_identifier.h"

namespace UniSphere {

class CompactRouter;
class NetworkSizeEstimator;

/**
 * Represents the sloppy group overlay manager.
 */
class UNISPHERE_EXPORT SloppyGroupManager {
public:
  /// Number of long-distance fingers to establish
  static const int finger_count = 1;
  /// Announce interval
  static const int interval_announce = 600;

  /**
   * Class constructor.
   *
   * @param router Router instance
   * @param sizeEstimator Size estimator instance
   */
  SloppyGroupManager(CompactRouter &router, NetworkSizeEstimator &sizeEstimator);

  SloppyGroupManager(const SloppyGroupManager&) = delete;
  SloppyGroupManager &operator=(const SloppyGroupManager&) = delete;

  /**
   * Initializes the sloppy group manager component.
   */
  void initialize();

  /**
   * Shuts down the sloppy group manager component.
   */
  void shutdown();

  /**
   * Outputs the sloppy group state to a stream.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dump(std::ostream &stream,
            std::function<std::string(const NodeIdentifier&)> resolve = nullptr);

  /**
   * Outputs the locally known sloppy group topology in DOT format.
   *
   * @param stream Output stream to dump into
   * @param resolve Optional name resolver
   */
  void dumpTopology(std::ostream &stream,
                    std::function<std::string(const NodeIdentifier&)> resolve = nullptr);
private:
  UNISPHERE_DECLARE_PRIVATE(SloppyGroupManager)
};

}

#endif
