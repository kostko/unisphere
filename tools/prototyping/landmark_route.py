from __future__ import print_function

import numpy
import math
import random
import hashlib
import networkx as nx
import topologies

# error in estimating n
n_err = 0.0

# probability that a Sybil node elects itself as a landmark
Psl = 0.5

# size multiplication factor
sm = 1

communities = dict(
  # honest region
  honest = dict(n=sm*334, degree=4, rewire=0.8),
  # sybil region
  sybil = dict(n=sm*666, degree=4, rewire=0.8),
  # foreign region
  foreign = dict(n=sm*333, degree=4, rewire=0.8),
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

# generate the trust topology
trust_topology = topologies.generate_topology(communities, connections, n_err, Psl)

def test01_random_routes(debug=True, iteration=1, store=True):
  vicinity_graph = nx.DiGraph()
  cn_graph = nx.DiGraph()
  group_graph = nx.DiGraph()

  color_neighbors = {}

  def dlog(*args):
    if debug:
      print(*args)

  # compute node vicinities
  dlog("vicinities")
  for idx, (node, data) in enumerate(trust_topology.nodes(data=True)):
    dlog("  - node", idx + 1)

    # setup node attributes
    vicinity_graph.add_node(node, data)
    group_graph.add_node(node, data)
    cn_graph.add_node(node, data)

    # determine color neighbors of a node; we expect O(log n) entries in each bucket
    dlog("    - color neighbors")
    buckets = color_neighbors.setdefault(node, {})
    # hop count determines priority within a bucket
    for _, neighbor in nx.bfs_edges(trust_topology, node):
      nd = trust_topology.node[neighbor]
      bucket = buckets.setdefault(nd['name'][:data['group_bits']], [])
      hops = nx.shortest_path_length(trust_topology, node, neighbor)
    
      if len(bucket) < data['cn_size']:
        bucket.append((hops, neighbor))
      else:
        bucket.sort()
        if (bucket[-1][0] > hops) or \
           (bucket[-1][0] == hops and random.random() < 0.5):
          bucket.pop()
          bucket.append((hops, neighbor))
        elif bucket[-1][0] < hops:
          # since in BFS traversal hop count is always increasing, we can
          # break as soon as we are beyond the maximum hop count in bucket
          break
    
    for vnodes in buckets.values():
      for hops, vnode in vnodes:
        cn_graph.add_edge(node, vnode, hops=hops)

    # determine vicinity of a node
    dlog("    - std. vicinity")
    vc = 0
    visited = set([node])
    queue = [node]
    while queue and vc < data['vicinity_size']:
      random.shuffle(queue)
      vnode = queue.pop()
      visited.add(vnode)

      for neighbor in trust_topology.neighbors(vnode):
        if neighbor in visited:
          continue

        visited.add(neighbor)
        vicinity_graph.add_edge(node, neighbor, hops=nx.shortest_path_length(trust_topology, node, neighbor))
        vc += 1
        if vc >= data['vicinity_size']:
          break

        queue.append(neighbor)

  dlog("groups")
  tnodes = trust_topology.nodes(data=True)
  random.shuffle(tnodes)
  for idx, (node, data) in enumerate(tnodes):
    dlog("  - node", idx + 1)
    max_hop_count = 0
    discovered_set = set([node])

    local_view = []
    foreign_view = []

    # find nodes in local vicinity
    dlog("    - vicinity")
    for vnode in vicinity_graph.neighbors(node):
      if not vicinity_graph.node[vnode]['name'].startswith(data['group']):
        continue

      hops = vicinity_graph.get_edge_data(node, vnode)['hops']
      max_hop_count = max(max_hop_count, hops)
      local_view.append((hops, vnode))
      discovered_set.add(vnode)

    dlog("    - color neighbors")
    for hops, vnode in color_neighbors[node].get(data['group'], []):
      if vnode in discovered_set:
        continue
    
      local_view.append((hops, vnode))
      discovered_set.add(vnode)

    # perform random walks to find distant nodes and establish links with
    # them to exchange data; perform log n random walks
    walk_length = int(round(1.5*max_hop_count))
    dlog("    - walk", walk_length)
    for _ in xrange(data['sg_degree']):
      wvisited = set()
      wnode = node
      whops = 0
      while whops < walk_length:
        wvisited.add(wnode)

        if vicinity_graph.node[wnode]['name'].startswith(data['group']) and \
           wnode not in discovered_set:
          hops = nx.shortest_path_length(trust_topology, node, wnode)
          foreign_view.append((hops, wnode))
          discovered_set.add(wnode)

        if not len(set(trust_topology.neighbors(wnode)) - wvisited):
          break

        # wvisited could be replaced by a bloom filter?
        while True:
          wnode = random.choice(trust_topology.neighbors(wnode))
          if wnode not in wvisited:
            break

    def add_group_link(node, vnode, hops):
      group_graph.add_edge(node, vnode, hops=hops, backlink=False)

      # check if vnode will also include this link as a back link; in the distributed
      # version this decision is performed at each node that receives an unknown announcement
      outgoing = [(d['hops'], v) for _, v, d in group_graph.out_edges(vnode, data=True) if d.get('backlink')]
      outgoing.sort()
      if len(outgoing) < data['sg_degree']**2:
        group_graph.add_edge(vnode, node, hops=hops, backlink=True)
      elif hops < outgoing[-1][0]:
        group_graph.remove_edge(vnode, outgoing[-1][1])
        group_graph.add_edge(vnode, node, hops=hops, backlink=True)

    # add all nodes from the local view (extended vicinity that matches our group)
    for hops, vnode in local_view:
      add_group_link(node, vnode, hops)

    # XXX: select far away neighbors
    if foreign_view and False:
      random.shuffle(foreign_view)
      long_neighbors = {}
      for hops, vnode in foreign_view:
        long_neighbors[hops] = vnode
        if len(long_neighbors) >= data['sg_degree']:
          break
    
      for hops, vnode in long_neighbors.items():
        add_group_link(node, vnode, hops)

    # what about name dynamics? DV protocol?

    # what about social dynamics? vicinity updated automatically, random walks can
    # be performed periodically?

  dlog("compute stats")
  in_degree = group_graph.in_degree().values()
  out_degree = group_graph.out_degree().values()
  degree = group_graph.degree().values()

  dlog("in_degree min/max/avg/std", min(in_degree), max(in_degree), numpy.average(in_degree), numpy.std(in_degree))
  dlog("out_degree min/max/avg/std", min(out_degree), max(out_degree), numpy.average(out_degree), numpy.std(out_degree))
  dlog("degree min/max/avg/std", min(degree), max(degree), numpy.average(degree), numpy.std(degree))
  for component in nx.connected_component_subgraphs(group_graph.to_undirected()):
    dlog("diameter", nx.diameter(component))

  if store:
    dlog("write output")
    nx.write_gml(vicinity_graph, 'vicinity%03d.gml' % iteration)
    nx.write_gml(group_graph, 'group%03d.gml' % iteration)
    nx.write_gml(cn_graph, 'cn%03d.gml' % iteration)
    nx.write_gml(trust_topology, 'trust%03d.gml' % iteration)

  # simulate announce flood from every node, sybil nodes drop all non-sybil announces
  # and propagate only sybil announces
  dlog("name dissemination simulation")
  records = {}
  for node, data in trust_topology.nodes(data=True):
    # initialize each node with its own record at the beginning
    records[node] = set([node])

  it = 0
  while True:
    it += 1
    dlog("iteration", it)
    convergence = True
    for node, data in trust_topology.nodes(data=True):
      # propagate learned records to all neighbors
      for record in records[node]:
        for neighbor in group_graph.successors(node):
          # sybil nodes will only propagate records for other sybils
          if trust_topology.node[neighbor]['sybil'] and not data['sybil']:
            continue

          rset = records[neighbor]
          if record not in rset:
            convergence = False
          rset.add(record)

    if convergence:
      break

  dlog("convergence")

  # now verify that all honest nodes know all the honest records in their group
  dlog("verification")
  mismatch = False
  for node, data in trust_topology.nodes(data=True):
    if data['sybil']:
      # ensure that sybils only know other sybils
      assert all([trust_topology.node[s]['sybil'] for s in records[node]])
      continue

    all_records = 0
    known_records = 0
    for pair, pdata in trust_topology.nodes(data=True):
      if pdata['sybil']:
        # we are not interested in how many sybil nodes are learned
        continue

      if pdata['name'].startswith(data['group']):
        all_records += 1
        if pair in records[node]:
          known_records += 1

    if known_records != all_records:
      print(node, "[%s]" % data['group'], "knows %d/%d (%.02f)" % (known_records, all_records, round(float(known_records) / all_records, 2)))
      mismatch = True

  if mismatch:
    # in case of mismatch, write the topology files
    dlog("write output")
    nx.write_gml(vicinity_graph, 'vicinity%03d.gml' % iteration)
    nx.write_gml(group_graph, 'group%03d.gml' % iteration)
    nx.write_gml(cn_graph, 'cn%03d.gml' % iteration)
    nx.write_gml(trust_topology, 'trust%03d.gml' % iteration)

test01_random_routes()

#for i in xrange(10000):
#  print("iteration", i + 1)
#  test01_random_routes(debug=False, iteration=i + 1, store=False)
