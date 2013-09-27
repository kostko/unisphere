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

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy
import pandas

class MessagingPerformance(base.PlotterBase):
  """
  Draws messaging performance of the USphere protocol.
  """
  def pre_process(self, data):
    """
    Performs pre-processing of the data so that the timeseries is grouped
    by timestamps and nodes in order to be processed correctly later on.
    """
    ts_base = min(data['ts'])
    groups = set(data['node_id'].unique())
    data.sort('ts', inplace=True)
    
    grouped_data = {}
    prev_ts = None
    current_ts = 0

    for _, element in data.iterrows():
      element_ts = element['ts'] - ts_base
      grouped_data.setdefault(element_ts, {})[element['node_id']] = element
      if current_ts != element_ts:
        # New timestamp, check if we have missed any nodes and include their previous
        # values in this timestamp -- this is to ensure that there are no weird jumps
        # when computing the rate of change
        if len(grouped_data[current_ts]) != len(groups):
          for group in groups.difference(grouped_data[current_ts].keys()):
            grouped_data[current_ts][group] = grouped_data[prev_ts][group]

        prev_ts = current_ts
        current_ts = element_ts

    timestamps = numpy.asarray(sorted(grouped_data.keys()))

    return timestamps, grouped_data

  def plot_variable(self, ax, timestamps, grouped_data, variable, color, label):
    """
    Plots a timeseries for a variable.
    """
    # Extract variable from the grouped dataset
    Y = [numpy.average([y[variable] for y in grouped_data[x].values()]) for x in timestamps]

    # Discard last 10 measurements
    X = timestamps[:-10]
    Y = Y[:-10]

    # Compute rate from absolute counter values
    Yrate = []
    for x1, x2, y1, y2 in zip(X, X[1:], Y, Y[1:]):
      Yrate += [(y2 - y1) / (x2 - x1)]
    
    X = X[1:]

    # Plot rate
    ax.plot(X, Yrate, color=color, alpha=0.4, zorder=0)

    # Also plot the moving average
    w = 60
    Ymean = numpy.asarray(pandas.rolling_mean(pandas.Series([0]*w + Yrate), w))
    ax.plot(X, Ymean[w:], label=label, color=color, zorder=1)

  def plot(self):
    """
    Plots the messaging performance.
    """
    fig, ax = plt.subplots()

    min_max_ts = None
    for i, run in enumerate(self.runs):
      # Load dataset
      data = run.get_dataset("stats-collect_performance-raw-*.csv")

      # Pre-process dataset so it gets properly grouped by timestamp for each node
      timestamps, grouped_data = self.pre_process(data)

      # Plot variables
      self.plot_variable(ax, timestamps, grouped_data, 'sg_msgs', mpl.cm.winter(float(i) / len(self.runs)),
        'SG msgs (n = %d)' % run.orig.settings.get('size', 0))
      self.plot_variable(ax, timestamps, grouped_data, 'rt_msgs', mpl.cm.autumn(float(i) / len(self.runs)),
        'RT msgs (n = %d)' % run.orig.settings.get('size', 0))

      # Compute the minimum of all maximum timestamps
      if min_max_ts is None or timestamps[-1] < min_max_ts:
        min_max_ts = timestamps[-1]

    ax.set_xlabel('time [s]')
    ax.set_ylabel('records/s')
    ax.set_xlim(0, min_max_ts)
    ax.grid()

    legend = ax.legend(loc='upper right', fontsize='small')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
