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
from .. import exceptions

import matplotlib.pyplot as plt
import numpy
import scipy.optimize

class AvailabilityVsAttackEdges(base.PlotterBase):
  """
  Draws deliverability and resolvability in relation to fraction of attack edges.
  """
  def plot(self):
    """
    Plots the deliverability and resolvability  vs. fraction of attack edges.
    """
    fig, ax = plt.subplots()

    values = {
      'resolved pairs': {},
      'deliverability': {},
    }
    for run in self.runs:
      # Load datasets
      data_deliverability = run.get_dataset("routing-pair_wise_ping-raw-*.csv")
      data_resolvability = run.get_dataset("sanity-check_consistent_ndb-report-*.csv")
      # Load input graph
      graph = run.get_graph("input-topology.graphml")

      # Extract deliverability information
      pairs = len(data_deliverability)
      delivered = data_deliverability['success'].sum()
      # Extract resolvability information
      resolved = float(data_resolvability['ratio'].sum())

      # Compute fraction of attack edges in the graph
      attack_edges = run.orig.settings['attack_edges']
      edge_fraction = round(attack_edges / float(graph.number_of_edges()), 2)

      values['deliverability'].setdefault(edge_fraction, []).append(delivered / float(pairs))
      values['resolved pairs'].setdefault(edge_fraction, []).append(resolved)

    dash = {
      'deliverability': (5, 5),
      'resolved pairs': (None, None),
    }
    for k, v in values.iteritems():
      X = sorted(v.keys())
      Y = [numpy.average(v[x]) for x in X]
      Yerr = [numpy.std(v[x]) for x in X]

      ax.errorbar(X, Y, Yerr, label=k.capitalize(), color='black', dashes=dash[k], marker='x')

    ax.set_xlabel('Percentage of attack edges')
    ax.grid()
    ax.axis((0.0, None, 0, 1.1))

    legend = ax.legend(loc='lower right')
    legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
