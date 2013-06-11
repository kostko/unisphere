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
  /// Scenario configuration
  boost::program_options::variables_map m_options;
};

ScenarioPrivate::ScenarioPrivate(const std::string &name)
  : m_name(name)
{
}

Scenario::Scenario(const std::string &name)
  : d(new ScenarioPrivate(name)),
    testbed(TestBed::getGlobalTestbed())
{
}

std::string Scenario::name() const
{
  return d->m_name;
}

void Scenario::setupOptions(int argc,
                            char **argv,
                            boost::program_options::options_description &options,
                            boost::program_options::variables_map &variables)
{
  boost::program_options::options_description local("Scenario " + d->m_name);
  setupOptions(local, variables);
  
  if (variables.empty())
    options.add(local);
  else
    d->m_options = variables;
}

}

}
