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

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy
import pandas

class MessagingPerformance(base.PlotterBase):
  """
  Draws messaging performance of the USphere protocol.
  """
  def pre_process(self, run, variables):
    """
    Performs pre-processing of the data so that the timeseries is grouped
    by timestamps and nodes in order to be processed correctly later on.
    """
    chunks = run.get_dataset("stats-collect_performance-raw-*.csv", chunksize=5000)
    ts_base, groups = None, set()
    for chunk in chunks:
      if ts_base is None:
        ts_base = min(chunk['ts'])
      else:
        ts_base = min(ts_base, min(chunk['ts']))

      groups.update(chunk['node_id'].unique())

    grouped_data = {}
    prev_ts = None
    current_ts = 0
    missing_values = []

    chunks = run.get_sorted_dataset("stats-collect_performance-raw-*.csv", "ts", chunksize=5000)
    for chunk in chunks:
      for _, element in chunk.iterrows():
        element_ts = element['ts'] - ts_base
        assert element_ts >= current_ts
        grouped_data.setdefault(element_ts, {})[element['node_id']] = element
        if current_ts != element_ts:
          # New timestamp, check if we have missed any nodes and include their previous
          # values in this timestamp -- this is to ensure that there are no weird jumps
          # when computing the rate of change
          if len(grouped_data[current_ts]) != len(groups):
            for group in groups.difference(grouped_data[current_ts].keys()):
              if prev_ts is not None:
                grouped_data[current_ts][group] = grouped_data[prev_ts][group]
              else:
                grouped_data[current_ts][group] = None

              if grouped_data[current_ts][group] is None:
                missing_values.append((current_ts, group))

          # Check if there were any missing values and fill them in as soon as we get the value
          # (to ensure that the rate of change is zero)
          for ts, group in missing_values:
            value = grouped_data[current_ts][group]
            if value is not None:
              grouped_data[ts][group] = value

          # Now that we have all data, compute averages for all values so we don't have
          # to keep this much data in memory
          if prev_ts is not None:
            group_sum = {}
            for v in variables:
              group_sum[v] = numpy.average([x[v] for x in grouped_data[prev_ts].itervalues()])
            grouped_data[prev_ts] = group_sum

          prev_ts = current_ts
          current_ts = element_ts

    timestamps = numpy.asarray(sorted(grouped_data.keys()))

    return timestamps, grouped_data

  def compute_rate(self, timestamps, grouped_data, variable):
    """
    Computes the rate of a variable.
    """
    # Extract variable from the grouped dataset
    X = timestamps[:-10]
    Y = [grouped_data[x][variable] for x in X]

    # Compute rate from absolute counter values
    Yrate = []
    for x1, x2, y1, y2 in zip(X, X[1:], Y, Y[1:]):
      Yrate += [(y2 - y1) / (x2 - x1)]
    
    return X[1:], Yrate

  def compute_average(self, X, Y, x_min):
    """
    Computes average and standard deviation.
    """
    Yf = [y for x, y in zip(X, Y) if x >= x_min]
    return numpy.average(Yf), numpy.std(Yf)

  def plot_variable(self, ax, X, Yrate, color, label):
    """
    Plots a timeseries for a variable.
    """
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

    # Determine the label attribute name
    label_attribute = self.graph.settings.get('label_attribute', 'size')

    min_max_ts = None
    averages = {
      'sg_msgs': {},
      'rt_msgs': {},
    }
    for i, run in enumerate(self.runs):
      run_attribute = run.orig.settings.get(label_attribute, 0)

      # Pre-process dataset so it gets properly grouped by timestamp for each node
      timestamps, grouped_data = self.pre_process(run, ['sg_msgs', 'rt_msgs'])

      # Compute rates
      sX, sYrate = self.compute_rate(timestamps, grouped_data, 'sg_msgs')
      rX, rYrate = self.compute_rate(timestamps, grouped_data, 'rt_msgs')

      # Plot variables
      self.plot_variable(ax, sX, sYrate, mpl.cm.winter(float(i) / len(self.runs)),
        'SG msgs (%s = %d)' % (label_attribute, run_attribute))
      self.plot_variable(ax, rX, rYrate, mpl.cm.autumn(float(i) / len(self.runs)),
        'RT msgs (%s = %d)' % (label_attribute, run_attribute))

      # Retrieve the moment in time when all nodes are considered up and compute average rates
      nodes_up_ts = run.get_marker("all_nodes_up")
      averages['sg_msgs'][run_attribute] = self.compute_average(sX, sYrate, nodes_up_ts)
      averages['rt_msgs'][run_attribute] = self.compute_average(rX, rYrate, nodes_up_ts)

      # Compute the minimum of all maximum timestamps
      if min_max_ts is None or timestamps[-1] < min_max_ts:
        min_max_ts = timestamps[-1]

    ax.set_xlabel('time [s]')
    ax.set_ylabel('records/s')
    ax.set_xlim(0, min_max_ts)
    ax.grid()

    if self.graph.settings.get('legend', True):
      legend = ax.legend(loc='upper right', fontsize='small')
      legend.get_frame().set_alpha(0.8)

    fig.savefig(self.get_figure_filename())

    # If there are multiple runs, plot the average records/s in relation to the variable
    if len(self.runs) > 1:
      ax.clear()

      for typ in averages:
        X = sorted(averages[typ].keys())
        Y = [averages[typ][x][0] for x in X]
        Yerr = [averages[typ][x][1] for x in X]
        ax.errorbar(X, Y, Yerr, marker='x', label=typ)

      ax.set_xlabel(label_attribute)
      ax.set_ylabel('records/s')
      ax.grid()

      legend = ax.legend(loc='upper right', fontsize='small')
      legend.get_frame().set_alpha(0.8)

      fig.savefig(self.get_figure_filename("avgs"))

