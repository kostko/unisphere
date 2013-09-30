import topologies
import math
import networkx as nx

# Generate graphs with fixed number of attack edges, but variable number of Sybil nodes
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

  G = topologies.generate_topology(communities, connections, 0.0, 0.0, simulation=False)
  nx.write_graphml(G, "sybil_s%d_n%d.graphml" % (size, n))

# Generate graphs with fixed number of Sybil nodes and variable number of attack edges
for edges in (60, 80, 100, 120, 140, 160, 180):
  communities = dict(
    # honest region
    honest = dict(n=64, degree=4, rewire=0.8),
    # sybil region
    sybil = dict(n=80, degree=4, rewire=0.8),
    # foreign region
    foreign = dict(n=64, degree=4, rewire=0.8),
  )

  # number of all nodes
  n = sum([c['n'] for c in communities.values()])

  connections = [
    # attack edges
    dict(src="sybil", dst="honest", count=edges),
    dict(src="sybil", dst="foreign", count=edges),
    # edges to foreign community
    dict(src="honest", dst="foreign", count=25),
  ]

  G = topologies.generate_topology(communities, connections, 0.0, 0.0, simulation=False)
  nx.write_graphml(G, "sybil_e%d_n%d.graphml" % (edges, n))
