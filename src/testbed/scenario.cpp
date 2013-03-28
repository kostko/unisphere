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
#include "testbed/scenario.h"
#include "testbed/test_bed.h"

namespace UniSphere {

namespace TestBed {

class ScenarioPrivate {
public:
  explicit ScenarioPrivate(const std::string &name);
public:
  /// Scenario name
  std::string m_name;
  /// Program options
  boost::program_options::options_description m_options;
};

ScenarioPrivate::ScenarioPrivate(const std::string &name)
  : m_name(name),
    m_options("Scenario options for " + name)
{
}

Scenario::Scenario(const std::string &name)
  : d(new ScenarioPrivate(name)),
    testbed(TestBed::getGlobalTestbed())
{
}

void Scenario::init()
{
  setupOptions(d->m_options);
}

std::string Scenario::name() const
{
  return d->m_name;
}

boost::program_options::options_description &Scenario::options()
{
  return d->m_options;
}

}

}
