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
import numpy
import statsmodels.api as sm

class PathStretchVsVariable(base.PlotterBase):
  """
  Draws path stretch growth in relation to a variable.
  """
  def plot(self):
    """
    Plots the path stretch growth.
    """
    fig, ax = plt.subplots()

    # Determine the label variable name
    variable = self.graph.settings.get('variable', 'size')

    averages = {}
    for run in self.runs:
      # Load dataset
      data = run.get_dataset("routing-pair_wise_ping-stretch-*.csv")

      # Extract stretch information
      data = data['stretch'].dropna()
      averages[run.orig.settings.get(variable, 0)] = (numpy.average(data), numpy.std(data))

    X = sorted(averages.keys())
    Y = [averages[x][0] for x in X]
    Yerr = [averages[x][1] for x in X]
    ax.errorbar(X, Y, Yerr, color='black', linestyle='-', marker='x')

    ax.set_xlabel(variable.capitalize())
    ax.set_ylabel('Path Stretch')
    ax.set_ylim(0, None)
    ax.grid()

    if self.graph.settings.get('scale'):
      ax.set_xscale(self.graph.settings.get('scale'))

    fig.savefig(self.get_figure_filename())
