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

import matplotlib.pyplot as plt
import statsmodels.api as sm

class LinkCongestion(base.PlotterBase):
  """
  Draws link load distribution over paths.
  """
  def plot(self):
    """
    Plots the CDF of link load.
    """
    fig, ax = plt.subplots()

    for run in self.runs:
      # Load datasets
      data_measure = run.get_dataset("stats-collect_link_congestion-raw-*.csv")
      data_sp = run.get_dataset("stats-collect_link_congestion-sp-*.csv")

      # Extract link congestion information
      data_measure = data_measure['msgs']
      data_sp = data_sp['msgs']

      # Compute ECDF and plot it
      ecdf_measure = sm.distributions.ECDF(data_measure)
      ecdf_sp = sm.distributions.ECDF(data_sp)

      ax.plot(ecdf_measure.x, ecdf_measure.y, drawstyle='steps', linewidth=2,
        label="UP (n = %d)" % run.orig.settings.get('size', 0))
      ax.plot(ecdf_sp.x, ecdf_sp.y, drawstyle='steps', linewidth=2,
        label="SP (n = %d)" % run.orig.settings.get('size', 0))

    ax.set_xlabel('Link Congestion')
    ax.set_ylabel('Cummulative Probability')
    ax.grid()
    ax.axis((0.5, None, 0, 1.01))
    self.convert_axes_to_bw(ax)

    legend = ax.legend(loc='lower right')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
