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
#ifndef UNISPHERE_MEASURE_MEASURE_H
#define UNISPHERE_MEASURE_MEASURE_H

#include "core/globals.h"

#include <vector>
#include <unordered_map>
#include <list>
#include <string>

namespace UniSphere {

/**
 * Metric is a result of a measurement.
 */
class UNISPHERE_EXPORT Metric {
public:
  /**
   * Constructs a metric.
   */
  Metric();
  
  /**
   * Increments the metric value for a specific amount. Assumes this metric
   * is a scalar one (only has a single value).
   * 
   * @param amount Amount to increment the metric for
   */
  void increment(int amount)
  {
    UniqueLock lock(m_mutex);
    m_values[0] += amount;
  }
  
  /**
   * Adds another measurement to this metric. Assumes this metric is a
   * multi-measurement metric.
   * 
   * @param value Measurement to add
   */
  void add(double value)
  {
    UniqueLock lock(m_mutex);
    m_values.push_back(value);
  }
  
  /**
   * Sets the metric value to a specific amount. Assumes this metric is
   * a scalar one.
   * 
   * @param value Value to set the metric to
   */
  void set(double value)
  {
    UniqueLock lock(m_mutex);
    m_values[0] = value;
  }

  /**
   * Return the metric's scalar value.
   */
  double value() const { return m_values[0]; }
  
  /**
   * Returns all the measurements of this metric.
   */
  const std::vector<double> &values() const { return m_values; }
private:
  /// Mutex protecting the metric
  std::mutex m_mutex;
  /// Metric value(s)
  std::vector<double> m_values;
};

UNISPHERE_SHARED_POINTER(Metric)

/**
 * Measure is a collection of metrics for a specific component.
 */
class UNISPHERE_EXPORT Measure {
public:
  /**
   * Constructs a new measure.
   * 
   * @param component Component name (defaults to "global")
   */
  Measure(const std::string &component = "global");

  /**
   * Returns the measure's component name.
   */
  inline std::string getComponent() const { return m_component; }

  /**
   * Increments the specific metric.
   * 
   * @param metric Metric name
   * @param amount Amount to increment the metric for
   */
  void increment(const std::string &metric, int amount = 1) { getMetric(metric)->increment(amount); }
  
  /**
   * Adds a new measurement of a specific metric.
   * 
   * @param metric Metric name
   * @param value Value to add
   */
  void add(const std::string &metric, double value) { getMetric(metric)->add(value); }
  
  /**
   * Sets the value of a specific metric.
   * 
   * @param metric Metric name
   * @param value Value to set
   */
  void set(const std::string &metric, double value) { getMetric(metric)->set(value); }
  
  /**
   * Returns the specified metric.
   * 
   * @param metric Metric name
   * @return Metric reference
   */
  MetricPtr getMetric(const std::string &metric);
  
  /**
   * Returns a list of metric names.
   */
  const std::list<std::string> getMetricNames() const;
private:
  /// Component name
  std::string m_component;
  /// Metric
  std::unordered_map<std::string, MetricPtr> m_metrics;
};

}

#endif
