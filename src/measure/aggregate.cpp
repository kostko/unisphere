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
#include "measure/aggregate.h"

namespace UniSphere {

AggregateMetric::AggregateMetric()
{
}

void AggregateMetric::add(const std::vector<double> &data)
{
  for (double datum : data) {
    m_accumulator(datum);
  }
}

void AggregateMeasure::add(Measure &measure)
{
  for (const std::string &metric : measure.getMetricNames()) {
    m_metrics[metric].add(measure.getMetric(metric)->values());
  }
}

}
