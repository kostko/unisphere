import topologies
import math
import networkx as nx

for size in (16, 32, 48, 64, 80, 96, 112, 128, 144, 160):
  communities = dict(
    # honest region
    honest = dict(n=64, degree=4, rewire=0.8),
    # sybil region
    sybil = dict(n=size, degree=4, rewire=0.8),
    # foreign region
    foreign = dict(n=64, degree=4, rewire=0.8),
  )

  # number of all nodes
  n = sum([c['n'] for c in communities.values()])

  connections = [
    # attack edges
    dict(src="sybil", dst="honest", count=10*int(math.log(n))),
    dict(src="sybil", dst="foreign", count=10*int(math.log(n))),
    # edges to foreign community
    dict(src="honest", dst="foreign", count=int(math.log(n))**2),
  ]

  G = topologies.generate_topology(communities, connections, 0.0, 0.0)
  nx.write_graphml(G, "sybil%d.graphml" % size)
