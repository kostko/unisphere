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

from . import base

import matplotlib.pyplot as plt
import statsmodels.api as sm

class DegreeDistribution(base.PlotterBase):
  """
  Draws degree distribution over nodes.
  """
  def plot(self):
    """
    Plots the CDF of node degrees.
    """
    fig, ax = plt.subplots()

    # Determine the label variable name
    variable = self.graph.settings.get('variable', 'size')

    for run in self.runs:
      # Load graph
      graph = run.get_graph(self.graph.settings['graph'])

      # Extract degree distribution information
      degrees = graph.degree().values()

      # Compute ECDF and plot it
      ecdf = sm.distributions.ECDF(degrees)

      ax.plot(ecdf.x, ecdf.y, drawstyle='steps', linewidth=2,
        label="%s = %d" % (variable, run.orig.settings.get(variable, 0)))

    ax.set_xlabel('Degree')
    ax.set_ylabel('Cummulative Probability')
    ax.grid()
    ax.axis((0.5, None, 0, 1.01))
    self.convert_axes_to_bw(ax)

    legend = ax.legend(loc='lower right')
    if self.settings.GRAPH_TRANSPARENCY:
      legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
