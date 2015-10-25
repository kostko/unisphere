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


class DeliverabilityVsAttackEdges(base.PlotterBase):
  """
  Draws deliverability in relation to fraction of attack edges.
  """

  def plot(self):
    """
    Plots the deliverability vs. fraction of attack edges.
    """

    fig, ax = plt.subplots()

    values = {}
    for run in self.runs:
      # Load dataset
      data = run.get_dataset("routing-pair_wise_ping-raw-*.csv")
      # Load input graph
      graph = run.get_graph("input-topology.graphml")

      # Extract deliverability information
      pairs = len(data)
      delivered = data['success'].sum()

      # Compute fraction of attack edges in the graph
      attack_edges = run.orig.settings['attack_edges']
      edge_fraction = round(attack_edges / float(graph.number_of_edges()), 2)

      values.setdefault(edge_fraction, []).append(delivered / float(pairs))

    X = sorted(values.keys())
    Y = [numpy.average(values[x]) for x in X]
    Yerr = [numpy.std(values[x]) for x in X]

    ax.errorbar(X, Y, Yerr, marker='x')
    ax.set_xlabel('Percentage of attack edges')
    ax.set_ylabel('Deliverability')
    ax.grid()
    ax.axis((0.0, None, 0, 1.01))

    fig.savefig(self.get_figure_filename())
