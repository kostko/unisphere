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

from .. import exceptions

import random
import networkx as nx


class Arguments(object):
  pass


def evaluate_argument(arg, args):
  """
  A helper function for lazy evaluation of arguments.
  """

  if callable(arg):
    return arg(args)
  return arg


def generate(topology, arguments, filename):
  """
  Generates a topology.
  """

  # Prepare the arguments that can be filled in
  args = Arguments()
  for arg in topology.args:
    setattr(args, arg, arguments.get(arg, None))

  trust_topology = nx.Graph()

  # Number of all nodes
  communities = evaluate_argument(topology.communities, args)
  n = sum([evaluate_argument(c['n'], args) for c in communities.values()])
  args.nodes = n

  # Generate communities
  community_graphs = {}
  for community, params in communities.items():
    community_size = evaluate_argument(params['n'], args)
    if community_size == 0:
      continue

    if params['type'] == 'watts-strogatz':
      graph = nx.connected_watts_strogatz_graph(
        community_size,
        evaluate_argument(params['degree'], args),
        evaluate_argument(params['rewire'], args)
      )
    elif params['type'] == 'holme-kim':
      graph = nx.powerlaw_cluster_graph(
        community_size,
        evaluate_argument(params['degree'], args),
        evaluate_argument(params['rewire'], args)
      )
    else:
      raise exceptions.ImproperlyConfigured('Community topology generator "type" not specified!')

    sybil = (community == "sybil")
    relabel = { node: "%s%s" % (community, node) for node in graph.nodes() }
    nx.relabel_nodes(graph, relabel, copy=False)
    trust_topology.add_nodes_from(graph, community=community, sybil=sybil)
    trust_topology.add_edges_from(graph.edges())
    community_graphs[community] = graph

  # Interconnect communities
  for connection in evaluate_argument(topology.connections, args):
    src = community_graphs.get(connection['src'], None)
    dst = community_graphs.get(connection['dst'], None)
    if src is None or dst is None:
      continue

    for i in xrange(evaluate_argument(connection['count'], args)):
      while True:
        # select random node in src
        snode = random.choice(src.nodes())
        # select random node in dst
        dnode = random.choice(dst.nodes())
        if not trust_topology.has_edge(snode, dnode):
          break

      # create an edge between them in trust topology
      trust_topology.add_edge(snode, dnode)

  # Assign identifiers to nodes, setup their status
  for node, data in trust_topology.nodes(data=True):
    data['sybil'] = int(data.get('sybil', False))
    data['label'] = str(node)

    trust_topology.add_node(node, data)

  nx.write_graphml(trust_topology, filename)
