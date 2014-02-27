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
from .. import exceptions

import matplotlib.pyplot as plt
import numpy
import scipy.optimize

class StateVsSize(base.PlotterBase):
  """
  Draws state growth in relation to topology size.
  """
  def plot(self):
    """
    Plots the state growth.
    """
    fig, ax = plt.subplots()

    values = {}
    for run in self.runs:
      # Load dataset
      data = run.get_dataset("stats-performance-raw-*.csv")

      # Extract values
      try:
        data = data[self.graph.settings['state']]
      except KeyError:
        raise exceptions.ImproperlyConfigured("State vs. size plot requires the 'state' tag to be set!")

      try:
        values[run.orig.settings['size']] = (numpy.average(data), numpy.std(data))
      except KeyError:
        raise exceptions.ImproperlyConfigured("State vs. size plot requires the 'size' tag to be set!")

    X = sorted(values.keys())
    Y = [values[x][0] for x in X]
    Yerr = [values[x][1] for x in X]

    ax.errorbar(X, Y, Yerr, marker='x', color='black', label='Measurements')

    # Fit a function over the measurements when configured
    fit_function = self.graph.settings.get('fit', None)
    if fit_function is not None:
      popt, pcov = scipy.optimize.curve_fit(fit_function, X, Y)
      Fx = numpy.linspace(min(X), max(X) + 1*(X[-1] - X[-2]), 100)
      Fy = [fit_function(x, *popt) for x in Fx]
      ax.plot(Fx, Fy, linestyle='--', color='black', label=self.graph.settings.get('fit_label', 'Fit'))

    ax.set_xlabel('Topology Size [nodes]')
    ax.set_ylabel('State at a Node [entries]')
    ax.grid()
    ax.set_ylim(0, None)

    if self.graph.settings.get('scale'):
      ax.set_xscale(self.graph.settings.get('scale'))

    legend = ax.legend(loc='lower right')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
