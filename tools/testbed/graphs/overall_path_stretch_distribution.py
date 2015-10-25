# -*- coding: utf-8 -*-
#
# This file is part of UNISPHERE.
#
# Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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

import matplotlib as mpl
import matplotlib.pyplot as plt
import statsmodels.api as sm


class OverallPathStretchDistribution(base.PlotterBase):
  """
  Draws path stretch distribution over paths.
  """

  def plot(self):
    """
    Plots the CDF of path stretch.
    """

    fig, ax = plt.subplots()

    for cfg in self.graph.settings['topologies']:
      count_all = 2.0
      count_current = count_all
      if 'colormap' in cfg:
        color = getattr(mpl.cm, cfg['colormap'])
      else:
        color = lambda _: cfg.get('color', 'black')

      for run in self.runs:
        if run.orig.settings['topology'].name in cfg['topology']:
          count_all += 1

      for run in self.runs:
        if run.orig.settings['topology'].name not in cfg['topology']:
          continue

        # Load dataset
        data = run.get_dataset("routing-pair_wise_ping-stretch-*.csv")
        data = data['stretch'].dropna()

        # Compute ECDF
        ecdf = sm.distributions.ECDF(data)

        legend_label = cfg.get('legend', None)
        variable = cfg.get('variable', None)
        if legend_label and variable:
          legend_label = legend_label % run.orig.settings[variable]

        ax.plot(ecdf.x, ecdf.y, drawstyle='steps', linewidth=2, color=color(count_current / count_all),
          label=legend_label)

        count_current += 1

    ax.set_xlabel('Razteg poti')
    ax.set_ylabel('Kumulativna verjetnost')
    ax.grid()
    ax.axis((0.5, None, 0, 1.01))
    #self.convert_axes_to_bw(ax)

    legend = ax.legend(loc='lower right')
    if self.settings.GRAPH_TRANSPARENCY:
      legend.get_frame().set_alpha(0.8)

    fig.savefig(self.get_figure_filename())
