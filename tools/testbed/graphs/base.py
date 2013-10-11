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

from .. import exceptions

import glob
import logging
import networkx as nx
import os
import pandas

logger = logging.getLogger('testbed.graphs.base')

class RunOutputDescriptor(object):
  def __init__(self, run_id, run, settings):
    self.run_id = run_id
    self.orig = run
    self.settings = settings

  def get_file(self, ds_expression):
    ds_path = os.path.join(self.settings.OUTPUT_DIRECTORY, self.run_id, self.orig.name, ds_expression)
    candidates = []
    for ds in glob.glob(ds_path):
      if not os.path.isfile(ds):
        continue

      candidates.append(ds)

    try:
      return sorted(candidates, reverse=True)[0]
    except IndexError:
      # Dataset does not exist
      logger.warning("Dataset matching '%s' does not exist for run '%s'!" %
        (ds_expression, self.orig.name))
      raise exceptions.MissingDatasetError

  def get_graph(self, ds_expression, **kwargs):
    return nx.read_graphml(self.get_file(ds_expression), **kwargs)

  def get_dataset(self, ds_expression, **kwargs):
    return pandas.read_csv(self.get_file(ds_expression), sep='\t', **kwargs)

class PlotterBase(object):
  def __init__(self, graph, run_id, runs, settings):
    self.graph = graph
    self.run_id = run_id
    self.runs = [RunOutputDescriptor(run_id, run, settings) for run in runs]
    self.settings = settings

  def get_figure_filename(self, suffix=None):
    fname = self.graph.name
    if suffix is not None:
      fname = "%s-%s" % (fname, suffix)

    return os.path.join(
      self.settings.OUTPUT_DIRECTORY,
      self.run_id,
      "graph-%s.%s" % (fname, self.settings.OUTPUT_GRAPH_FORMAT)
    )

  def convert_axes_to_bw(self, ax):
    """
    Take each Line2D in the axes, ax, and convert the line style to be 
    suitable for black and white viewing.
    """
    MARKERSIZE = 3
    COLORMAP = {
      'b': {'marker': None, 'dash': (None,None)},
      'g': {'marker': None, 'dash': [5,5]},
      'r': {'marker': None, 'dash': [5,3,1,3]},
      'c': {'marker': None, 'dash': [1,3]},
      'm': {'marker': None, 'dash': [5,2,5,2,5,10]},
      'y': {'marker': None, 'dash': [5,3,1,2,1,10]},
      'k': {'marker': 'o', 'dash': (None,None)} #[1,2,1,10]}
    }

    for line in ax.get_lines():
      color = line.get_color()
      line.set_color('black')
      line.set_dashes(COLORMAP[color]['dash'])
      line.set_marker(COLORMAP[color]['marker'])
      line.set_markersize(MARKERSIZE)

  def plot(self):
    raise NotImplementedError
