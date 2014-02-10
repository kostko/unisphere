/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_SOCIAL_SIZEESTIMATOR_H
#define UNISPHERE_SOCIAL_SIZEESTIMATOR_H

#include <cstdint>
#include <boost/signals2/signal.hpp>

namespace UniSphere {

/**
 * An interface for network size estimators.
 */
class NetworkSizeEstimator {
public:
  /**
   * Returns the network size estimation.
   */
  virtual std::uint64_t getNetworkSize() const = 0;
public:
  /// Signal that gets emitted when the estimated network size is changed
  boost::signals2::signal<void(std::uint64_t)> signalSizeChanged;
};

/**
 * An estimator that knows the exact network size.
 */
class OracleNetworkSizeEstimator : public NetworkSizeEstimator {
public:
  /**
   * Class constructor.
   *
   * @param size Network size
   */
  OracleNetworkSizeEstimator(std::uint64_t size) : m_size(size) {}

  /**
   * Returns the network size estimation.
   */
  std::uint64_t getNetworkSize() const { return m_size; }
private:
  /// Known network size
  std::uint64_t m_size;
};

}

#endif
