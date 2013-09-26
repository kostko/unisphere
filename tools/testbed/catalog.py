#
# This file is part of UNISPHERE.
#
# Copyright (C) 2013 Jernej Kos <k@jst.sm>
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import collections
import logging

logger = logging.getLogger('testbed.catalog')

class RunDescriptor(object):
  def __init__(self, name, settings):
    """
    Class constructor.

    :param name: Run name
    :param settings: A dictionary of run settings
    """
    self.name = name
    self.settings = settings.copy()
    del self.settings['name']

  def run(self, cluster):
    """
    Prepares the cluster and starts the scenario.

    :param cluster: Cluster configurator instance
    """
    try:
      logger.info("Setting up cluster...")
      cluster.setup(self)

      logger.info("Running scenario on cluster...")
      cluster.run_scenario(self)

      logger.info("Shutting down cluster...")
      cluster.shutdown()
    except:
      logger.error("An error has ocurred while running the scenario!")
      cluster.shutdown()
      raise

class RunCatalog(object):
  """
  The run catalog contains all run configuration.
  """
  def __init__(self):
    """
    Class constructor.
    """
    self._runs = collections.OrderedDict()

  def load(self, settings):
    """
    Loads the run configuration from user settings.

    :param settings: User settings dictionary
    """
    # Iterate over configured runs
    for run in settings.RUNS:
      self._runs[run['name']] = RunDescriptor(run['name'], run)

    logger.info("Loaded %d run descriptors." % len(self._runs))

  def __iter__(self):
    """
    Returns an iterator over the run collection.
    """
    return iter(self._runs.values())

catalog = RunCatalog()
