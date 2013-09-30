import math
import random
import hashlib
import networkx as nx

def generate_topology(communities, connections, n_err, Psl, simulation=True):
  trust_topology = nx.Graph()

  # number of all nodes
  n = sum([c['n'] for c in communities.values()])

  # generate communities
  community_graphs = {}
  for community, params in communities.items():
    graph = nx.connected_watts_strogatz_graph(params['n'], params['degree'], params['rewire'])
    sybil = (community == "sybil")
    relabel = { node: "%s%s" % (community, node) for node in graph.nodes() }
    nx.relabel_nodes(graph, relabel, copy=False)
    trust_topology.add_nodes_from(graph, community=community, sybil=sybil)
    trust_topology.add_edges_from(graph.edges())
    community_graphs[community] = graph

  # interconnect communities
  for connection in connections:
    src = community_graphs[connection['src']]
    dst = community_graphs[connection['dst']]

    for i in xrange(connection['count']):
      while True:
        # select random node in src
        snode = random.choice(src.nodes())
        # select random node in dst
        dnode = random.choice(dst.nodes())
        if not trust_topology.has_edge(snode, dnode):
          break

      # create an edge between them in trust topology
      trust_topology.add_edge(snode, dnode)

  # assign identifiers to nodes, setup their status
  landmarks = []
  for node, data in trust_topology.nodes(data=True):
    # compute approximate n
    n_hat = n + (2*(random.random() - 0.5) * n_err * n)

    nid = bin(int(hashlib.sha1(str(node)).hexdigest(), 16))[2:]
    nid = ("0" * (160 - len(nid))) + nid
    # select node group prefix
    group = nid[:int(math.floor(math.log(math.sqrt(n_hat / math.log(n_hat)), 2)))]

    data['sybil'] = int(data.get('sybil', False))
    data['label'] = str(node)
    if simulation:
      data['name'] = nid
      data['group'] = group
      data['group_bits'] = len(group)
      data['vicinity_size'] = int(math.sqrt(n_hat * math.log(n_hat)))
      data['sg_degree'] = int(math.log(n_hat))
      data['cn_size'] = int(math.log(n_hat))

      is_landmark = False
      if data['sybil']:
        is_landmark = random.random() < Psl
      else:
        is_landmark = random.random() < math.sqrt(math.log(n_hat) / n_hat)

      if is_landmark:
        data['landmark'] = 1
        landmarks.append(node)
      else:
        data['landmark'] = 0

    trust_topology.add_node(node, data)

  return trust_topology
