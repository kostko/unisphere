#
# This file is part of UNISPHERE.
#
# Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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

from .topologies import generator
from . import exceptions

import collections
import fnmatch
import importlib
import logging

logger = logging.getLogger('testbed.catalog')

class TopologyDescriptor(object):
  def __init__(self, settings):
    """
    Class constructor.

    :param settings: A dictionary of topology settings
    """

    self.settings = settings

    if not self.name:
      raise exceptions.ImproperlyConfigured("Topology is missing a name!")
    if not self.generator:
      raise exceptions.ImproperlyConfigured("Topology '%s' has no generator defined!" % self.name)

    # Attempt to load the topology generator
    generator_module = "testbed.%s" % self.generator
    i = generator_module.rfind('.')
    module, attr = generator_module[:i], generator_module[i + 1:]
    try:
      module = importlib.import_module(module)
      self.settings['generator'] = getattr(module, attr)
      self.generator.name = generator_module
    except (ImportError, AttributeError):
      raise exceptions.ImproperlyConfigured("Error importing generator module '%s'!" % generator_module)

  @property
  def generator(self):
    return self.settings.get('generator', None)

  @property
  def name(self):
    return self.settings.get('name', None)

  @property
  def args(self):
    return self.settings.get('args', [])

  @property
  def communities(self):
    return self.settings.get('communities', None)

  @property
  def connections(self):
    return self.settings.get('connections', [])

  def generate(self, run, filename):
    """
    Generates a topology.

    :param run: Run descriptor
    :param filename: Output graph filename
    """

    try:
      logger.info("Generating topology '%s' using '%s'..." % (self.name, self.generator.name))
      self.generator(self, run.settings, filename)
    except:
      logger.error("An error has ocurred while generating the topology!")
      raise

  def __str__(self):
    """
    Returns the topology name.
    """

    return self.name

class RunDescriptor(object):
  def __init__(self, settings):
    """
    Class constructor.

    :param settings: A dictionary of run settings
    """
    self.name = settings['name']
    self.settings = settings.copy()
    del self.settings['name']

  def generate_topology(self, filename):
    """
    Generates a topology for this run.

    :param filename: Output graph filename
    """
    self.settings['topology'].generate(self, filename)

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
  def __init__(self, settings):
    """
    Class constructor.

    :param settings: A dictionary of graph settings
    """
    self.name = settings['name']
    self.plotter = settings['plotter']
    self.runs = settings['runs']
    self.settings = settings.copy()
    del self.settings['name']
    del self.settings['plotter']
    del self.settings['runs']

  def plot(self, run_id, runs, settings):
    """
    Plots the graph.

    :param run_id: Run group identifier
    :param runs: A list of dependent run descriptors
    :param settings: User settings
    """
    try:
      # Create a new plotter instance
      plotter = self.plotter(self, run_id, runs, settings)

      # Plot the graph
      plotter.plot()
    except:
      logger.error("An error has ocurred while plotting the graph!")
      raise

class Catalog(object):
  """
  The catalog contains all topology, run and graph configuration.
  """
  def __init__(self):
    """
    Class constructor.
    """
    self._topologies = collections.OrderedDict()
    self._runs = collections.OrderedDict()
    self._graphs = collections.OrderedDict()

  def load(self, settings, graphs=True):
    """
    Loads the topology, run and graph configuration from user settings.

    :param settings: User settings dictionary
    :param graphs: Should we also load graphs
    """
    # Iterate over configured topologies
    for topology in settings.TOPOLOGIES:
      self._topologies[topology['name']] = TopologyDescriptor(topology)

    # Iterate over configured runs
    apply_cfg = {}
    for run_cfg in settings.RUNS:
      # Check if the run configuration is actually an apply_to directive
      if run_cfg.get('apply_to', None) is not None:
        run = run_cfg.copy()
        del run['apply_to']
        for pattern in run_cfg['apply_to']:
          apply_cfg.setdefault(pattern, []).append(run)
        continue
      else:
        # Check if any apply directives match this run
        for pattern, cfgs in apply_cfg.iteritems():
          if fnmatch.fnmatch(run_cfg['name'], pattern):
            for cfg in cfgs:
              run_cfg.update(cfg)

      # Check if we need to create multiple repeats of the same run (but with a changed name)
      repeats = []
      if run_cfg.get('repeats', None) is not None:
        for i in xrange(1, run_cfg['repeats'] + 1):
          run = run_cfg.copy()
          run['name'] = "%s.%d" % (run['name'], i)
          repeats.append(run)
      else:
        repeats.append(run_cfg)

      for run in repeats:
        try:
          topology = self._topologies[run['topology']]
        except KeyError:
          raise exceptions.ImproperlyConfigured("Topology generator named '%s' is not configured!" %
            run['topology'])

        run['topology'] = topology
        self._runs[run['name']] = RunDescriptor(run)

    # Iterate over configured graphs
    if graphs:
      for graph in settings.GRAPHS:
        plotter_module = "testbed.%s" % graph['plotter']
        i = plotter_module.rfind('.')
        module, attr = plotter_module[:i], plotter_module[i + 1:]
        try:
          module = importlib.import_module(module)
          graph['plotter'] = getattr(module, attr)
        except (ImportError, AttributeError):
          raise exceptions.ImproperlyConfigured("Error importing plotter module '%s'!" % plotter_module)

        # Automatically resolve run names that represent glob expressions
        runs = []
        for run_name in graph['runs']:
          for run in self.runs():
            if fnmatch.fnmatch(run.name, run_name):
              runs.append(run.name)
        graph['runs'] = runs

        self._graphs[graph['name']] = GraphDescriptor(graph)

    logger.info("Loaded %d topology, %d run and %d graph descriptors." %
      (len(self._topologies), len(self._runs), len(self._graphs)))

  def topologies(self):
    """
    Returns an iterator over the topologies collection.
    """
    return iter(self._topologies.values())

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
