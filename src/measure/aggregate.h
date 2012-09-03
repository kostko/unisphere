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
#ifndef UNISPHERE_MEASURE_AGGREGATE_H
#define UNISPHERE_MEASURE_AGGREGATE_H

#include "core/globals.h"
#include "measure/measure.h"

#include <vector>
#include <unordered_map>
#include <cmath>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>

namespace UniSphere {

/**
 * Metric that aggregates multiple values and extracts statistics like
 * mean and standard deviation. It uses the Boost Accumulators framework
 * to perform this aggregation in an incremental fashion.
 */
class UNISPHERE_EXPORT AggregateMetric {
public:
  /**
   * Class constructor.
   */
  AggregateMetric();
  
  /**
   * Adds more data points to be aggregated.
   * 
   * @param data Data points
   */
  void add(const std::vector<double> &data);
  
  /**
   * Returns the mean of the data points.
   */
  double mean() const { return boost::accumulators::mean(m_accumulator); }
  
  /**
   * Returns the standard deviation of the data points.
   */
  double std() const  { return sqrt(boost::accumulators::variance(m_accumulator)); }
  
  /**
   * Returns the minimum of the data points.
   */
  double min() const { return boost::accumulators::min(m_accumulator); }
  
  /**
   * Returns the maximum of the data points.
   */
  double max() const { return boost::accumulators::max(m_accumulator); }
private:
  /// Accumulator for computing aggregates
  boost::accumulators::accumulator_set<
    double,
    boost::accumulators::stats<
      boost::accumulators::tag::mean,
      boost::accumulators::tag::variance,
      boost::accumulators::tag::min,
      boost::accumulators::tag::max
    >
  > m_accumulator;
};

/**
 * Aggregate of multiple measures providing aggregate metrics.
 */
class UNISPHERE_EXPORT AggregateMeasure {
public:
  /**
   * Adds a new measure to this aggregate.
   * 
   * @param measure Measure to add
   */
  void add(Measure &measure);
  
  /**
   * Returns a mapping of aggregate metrics.
   */
  const std::unordered_map<std::string, AggregateMetric> &metrics() const { return m_metrics; }
private:
  /// Resulting metrics
  std::unordered_map<std::string, AggregateMetric> m_metrics;
};

}

#endif
