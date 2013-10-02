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

from . import base
from .. import exceptions

import matplotlib.pyplot as plt
import numpy
import scipy.optimize

class DegreeVsVariable(base.PlotterBase):
  """
  Draws graph degree in relation to some variable.
  """
  def plot(self):
    """
    Plots the degree vs. variable.
    """
    fig, ax = plt.subplots()

    # Determine the label variable name
    variable = self.graph.settings.get('variable', 'size')

    values = {}
    for run in self.runs:
      # Load graph
      graph = run.get_graph(self.graph.settings['graph'])

      # Extract degree distribution information
      degrees = graph.degree().values()
      values[run.orig.settings[variable]] = (numpy.average(degrees), numpy.std(degrees))

    X = sorted(values.keys())
    Y = [values[x][0] for x in X]
    Yerr = [values[x][1] for x in X]

    ax.errorbar(X, Y, Yerr, label='Measurements')

    # Fit a function over the measurements when configured
    fit_function = self.graph.settings.get('fit', None)
    if fit_function is not None:
      popt, pcov = scipy.optimize.curve_fit(fit_function, X, Y)
      Fx = numpy.linspace(min(X), max(X) + 2*(X[-1] - X[-2]), 100)
      Fy = [fit_function(x, *popt) for x in Fx]
      ax.plot(Fx, Fy, linestyle='--', color='black', label=self.graph.settings.get('fit_label', 'Fit'))

    ax.set_xlabel(variable.capitalize())
    ax.set_ylabel('Degree')
    ax.grid()

    legend = ax.legend(loc='lower right')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
