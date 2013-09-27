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

from .. import exceptions

import glob
import logging
import os
import pandas

logger = logging.getLogger('testbed.graphs.base')

class RunOutputDescriptor(object):
  def __init__(self, run_id, run, settings):
    self.run_id = run_id
    self.orig = run
    self.settings = settings

  def get_dataset(self, ds_expression):
    ds_path = os.path.join(self.settings.OUTPUT_DIRECTORY, self.run_id, self.orig.name, ds_expression)
    candidates = []
    for ds in glob.glob(ds_path):
      if not os.path.isfile(ds):
        continue

      candidates.append(ds)

    try:
      return pandas.read_csv(sorted(candidates, reverse=True)[0], sep=None, engine='python')
    except IndexError:
      # Dataset does not exist
      logger.warning("Dataset matching '%s' does not exist for run '%s'!" %
        (ds_expression, self.orig.name))
      raise exceptions.MissingDatasetError

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

  def plot(self):
    raise NotImplementedError
