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
#include "measure/measure.h"

namespace UniSphere {
  
Metric::Metric()
{
  m_values.push_back(0.0);
}

Measure::Measure(const std::string &component)
  : m_component(component)
{
}

MetricPtr Measure::getMetric(const std::string &metric)
{
  MetricPtr m;
  if (m_metrics.find(metric) == m_metrics.end()) {
    m = MetricPtr(new Metric);
    m_metrics[metric] = m;
  } else {
    m = m_metrics[metric];
  }
  
  return m;
}

const std::list<std::string> Measure::getMetricNames() const
{
  std::list<std::string> metrics;
  for (const auto &p : m_metrics) {
    metrics.push_back(p.first);
  }
  return metrics;
}
  
}
