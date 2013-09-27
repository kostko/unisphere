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

from .graphs import base as graphs_base
from . import exceptions

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

  def run(self, cluster, run_id):
    """
    Prepares the cluster and starts the scenario.

    :param cluster: Cluster configurator instance
    :param run_id: Unique identifier for the run session
    """
    try:
      logger.info("Setting up cluster...")
      cluster.setup(self, run_id)

      logger.info("Running scenario on cluster...")
      cluster.run_scenario(self, run_id)

      logger.info("Shutting down cluster...")
      cluster.shutdown()
    except:
      logger.error("An error has ocurred while running the scenario!")
      cluster.shutdown()
      raise

class GraphDescriptor(object):
  def __init__(self, name, plotter, runs):
    """
    Class constructor.

    :param name: Run name
    :param plotter: A valid plotter class
    :param runs: Run dependencies
    """
    self.name = name
    self.plotter = plotter
    self.runs = set(runs)

  def plot(self, run_id, runs, settings):
    """
    Plots the graph.

    :param run_id: Run group identifier
    :param runs: A list of dependent run descriptors
    :param settings: User settings
    """
    try:
      # Create a new plotter instance
      plotter = self.plotter(run_id, runs, settings)

      # Plot the graph
      plotter.plot()
    except:
      logger.error("An error has ocurred while plotting the graph!")
      raise

class Catalog(object):
  """
  The catalog contains all run and graph configuration.
  """
  def __init__(self):
    """
    Class constructor.
    """
    self._runs = collections.OrderedDict()
    self._graphs = collections.OrderedDict()

  def load(self, settings):
    """
    Loads the run and graph configuration from user settings.

    :param settings: User settings dictionary
    """
    # Iterate over configured runs
    for run in settings.RUNS:
      self._runs[run['name']] = RunDescriptor(run['name'], run)

    # Iterate over configured graphs
    for graph in settings.GRAPHS:
      if not issubclass(graph['plotter'], graphs_base.PlotterBase):
        raise exceptions.ImproperlyConfigured("Graph plotter for '%s' is not a testbed.graphs.base.PlotterBase subclass!" %
          (graph['name']))

      self._graphs[graph['name']] = GraphDescriptor(graph['name'], graph['plotter'], graph['runs'])

    logger.info("Loaded %d run and %d graph descriptors." % (len(self._runs), len(self._graphs)))

  def runs(self):
    """
    Returns an iterator over the run collection.
    """
    return iter(self._runs.values())

  def graphs(self):
    """
    Returns an iterator over the graphs collection.
    """
    return iter(self._graphs.values())

  def get_run_descriptors(self, run_names):
    """
    Returns run descriptors for specified run names.
    """
    return [self._runs[name] for name in run_names]

catalog = Catalog()
