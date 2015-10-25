# -*- coding: utf-8 -*-
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


class LRLengthVsVariable(base.PlotterBase):
  """
  Draws L-R address length in relation to some variable.
  """

  def plot(self):
    """
    Plots the L-R address length vs. variable.
    """

    fig, ax = plt.subplots()

    # Determine the label variable name
    variable = self.graph.settings.get('variable', 'size')
    variable_label = self.graph.settings.get('variable_label', variable.capitalize().replace('_', ' '))

    values = {
      'primary': {},
      'secondary': {},
    }
    for run in self.runs:
      # Load datasets
      data_primary = run.get_dataset("stats-lr_address_lengths-primary-*.csv")
      data_secondary = run.get_dataset("stats-lr_address_lengths-secondary-*.csv")

      # Extract length information
      primary = data_primary['length']
      secondary = data_secondary['length']

      values['primary'][run.orig.settings[variable]] = (numpy.average(primary), numpy.std(primary))
      values['secondary'][run.orig.settings[variable]] = (numpy.average(secondary), numpy.std(secondary))

    dash = {
      'primary': (None, None),
      'secondary': (5, 5),
    }

    labels = {
      'primary': 'Primarni',
      'secondary': 'Sekundarni',
    }

    for typ in values:
      X = sorted(values[typ].keys())
      Y = [values[typ][x][0] for x in X]
      Yerr = [values[typ][x][1] for x in X]
      ax.errorbar(X, Y, Yerr, marker='x', color='black', dashes=dash[typ], label=labels[typ])

    ax.set_xlabel(variable_label)
    ax.set_ylabel(u'Dolžina naslova L-R')
    ax.grid()

    if self.graph.settings.get('scale'):
      ax.set_xscale(self.graph.settings.get('scale'))

    legend = ax.legend(loc='upper left')
    if self.settings.GRAPH_TRANSPARENCY:
      legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
