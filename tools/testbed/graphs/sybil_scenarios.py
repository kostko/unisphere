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

import matplotlib.pyplot as plt
import numpy


class SybilScenarios(base.PlotterBase):
  """
  Draws deliverability and resolvability in relation to fraction of attack edges.
  """

  def plot(self):
    """
    Plots the deliverability and resolvability  vs. fraction of attack edges.
    """

    fig, ax = plt.subplots()

    values = {
      'scenario_a': {},
      'scenario_b': {},
      'scenario_c': {},
    }
    for run in self.runs:
      # Load input graph
      graph = run.get_graph("input-topology.graphml")

      # Compute fraction of attack edges in the graph
      attack_edges = run.orig.settings['attack_edges']
      edge_fraction = round(attack_edges / float(graph.number_of_edges()), 2)

      # Obtain scenario name
      scenario = run.orig.settings['scenario']

      if scenario in ("SybilNodesNames", "SybilNodesNamesLandmarks"):
        data_resolvability = run.get_dataset("sanity-check_consistent_ndb-report-*.csv")

        # Extract resolvability information
        resolved = float(data_resolvability['ratio'].sum())

        if scenario == "SybilNodesNames":
          scenario_key = 'scenario_a'
        elif scenario == "SybilNodesNamesLandmarks":
          scenario_key = 'scenario_b'

        values[scenario_key].setdefault(edge_fraction, []).append(resolved)
      elif scenario == "SybilNodesRouting":
        data_deliverability = run.get_dataset("routing-pair_wise_ping-raw-*.csv")

        # Extract deliverability information
        pairs = len(data_deliverability)
        delivered = data_deliverability['success'].sum()

        values['scenario_c'].setdefault(edge_fraction, []).append(delivered / float(pairs))

    designs = [
      ('scenario_a', {'label': "Preslikani pari (scenarij A)", 'dashes': (None, None), 'marker': '^', 'color': 'black'}),
      ('scenario_b', {'label': "Preslikani pari (scenarij B)", 'dashes': (5, 3, 1, 3), 'marker': 'v', 'color': 'red'}),
      ('scenario_c', {'label': u"Dostavljena sporočila (scenarij C)", 'dashes': (5, 5), 'marker': 'x', 'color': 'blue'}),
    ]

    for scenario_name, design in designs:
      data = values[scenario_name]
      X = sorted(data.keys())
      Y = [numpy.average(data[x]) for x in X]
      Yerr = [numpy.std(data[x]) for x in X]

      ax.errorbar(X, Y, Yerr, **design)

    ax.set_xlabel(u'Delež napadenih povezav')
    ax.grid()
    ax.axis((0.0, None, 0, 1.1))

    legend = ax.legend(loc='lower right')
    if self.settings.GRAPH_TRANSPARENCY:
      legend.get_frame().set_alpha(0.8)
    fig.savefig(self.get_figure_filename())
