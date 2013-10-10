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

class LRLengthDistribution(base.PlotterBase):
  """
  Draws L-R address length distribution over nodes.
  """
  def plot(self):
    """
    Plots the CDF of L-R address length distribution.
    """
    fig, ax = plt.subplots()

    for run in self.runs:
      # Load dataset
      data_primary = run.get_dataset("stats-lr_address_lengths-primary-*.csv")
      data_secondary = run.get_dataset("stats-lr_address_lengths-secondary-*.csv")

      # Compute ECDF and plot it
      ecdf_primary = sm.distributions.ECDF(data_primary['length'])
      ecdf_secondary = sm.distributions.ECDF(data_secondary['length'])

      ax.plot(ecdf_primary.x, ecdf_primary.y, drawstyle='steps', linewidth=2,
        label="Primary (n = %d)" % run.orig.settings.get('size', 0))
      ax.plot(ecdf_secondary.x, ecdf_secondary.y, drawstyle='steps', linewidth=2,
        label="Secondary (n = %d)" % run.orig.settings.get('size', 0))

    ax.set_xlabel('L-R Address Length')
    ax.set_ylabel('Cummulative Probability')
    ax.grid()
    ax.axis((0.5, None, 0, 1.01))
    self.convert_axes_to_bw(ax)

    legend = ax.legend(loc='lower right')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
