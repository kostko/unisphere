
import math
import numpy
import os

TESTBED_ROOT = os.path.realpath(os.path.join(os.path.abspath(os.path.dirname(__file__)), ".."))
TESTBED_BINARY = os.path.join(TESTBED_ROOT, "build/release/apps/testbed/testbed")
DATA_DIRECTORY = os.path.join(TESTBED_ROOT, "data")
OUTPUT_DIRECTORY = os.path.join(TESTBED_ROOT, "output")
OUTPUT_GRAPH_FORMAT = "pdf"

# Cluster configuration
CLUSTER = {
  'testbed.cluster.simple.SimpleCluster': {
    'master_ip': '127.0.0.1',
    'controller_ip': '127.0.0.2',
    'slave_ip': '127.0.1.%d',
    'slave_sim_ip': '127.0.2.%d',
    'slave_sim_ports': (9000, 20000),
  }
}

# Configure topologies
TOPOLOGIES = [
  # A single community but a custom number of nodes
  dict(
    name="basic_single",
    args=['size'],
    communities=dict(
      honest=dict(n=lambda a: a.size, degree=4, rewire=0.8),
    ),
  ),

  # A fixed number of nodes, but multiple communities
  dict(
    name="basic_multi",
    args=['size', 'communities', 'degree'],
    communities=lambda a: dict([
      ("c%d" % i, dict(n=a.size // a.communities, degree=a.degree, rewire=0.8))
      for i in xrange(a.communities)
    ]),
    connections=lambda a: [
      dict(src="c%d" % x, dst="c%d" % y, count=lambda a: max(1, int(math.log(a.nodes) / a.communities)))
      for x in xrange(a.communities)
      for y in xrange(a.communities)
      if x < y
    ],
  ),

  # Varying number of Sybil nodes, but number of edges and sizes of honest/foreign
  # communities stay the same
  dict(
    name="sybil_count",
    args=['sybils'],
    communities=dict(
      honest=dict(n=64, degree=4, rewire=0.8),
      sybil=dict(n=lambda a: a.sybils, degree=4, rewire=0.8),
      foreign=dict(n=64, degree=4, rewire=0.8),
    ),
    connections=[
      # Attack edges
      dict(src="sybil", dst="honest", count=lambda a: int(math.log(a.nodes))),
      dict(src="sybil", dst="foreign", count=lambda a: int(math.log(a.nodes))),
      # Edges to foreign community
      dict(src="honest", dst="foreign", count=lambda a: int(math.log(a.nodes))),
    ],
  ),

  # Varying number of attack edges, but size of all communities stays the same
  dict(
    name="sybil_edges",
    args=['attack_edges'],
    communities=dict(
      honest=dict(n=64, degree=4, rewire=0.8),
      sybil=dict(n=64, degree=4, rewire=0.8),
      foreign=dict(n=64, degree=4, rewire=0.8),
    ),
    connections=[
      # Attack edges
      dict(src="sybil", dst="honest", count=lambda a: a.attack_edges),
      dict(src="sybil", dst="foreign", count=lambda a: a.attack_edges),
      # Edges to foreign community
      dict(src="honest", dst="foreign", count=25),
    ],
  ),
]

# Configure test runs
RUNS = [
  # Performance measurement runs
  dict(name="pf-b1", topology="basic_single", size=16, scenario="StandardTests"),
  dict(name="pf-b2", topology="basic_single", size=32, scenario="StandardTests"),
  dict(name="pf-b3", topology="basic_single", size=64, scenario="StandardTests"),
  dict(name="pf-b4", topology="basic_single", size=128, scenario="StandardTests"),
  dict(name="pf-b5", topology="basic_single", size=256, scenario="StandardTests"),
  dict(name="pf-b6", topology="basic_single", size=512, scenario="StandardTests"),
  dict(name="pf-b7", topology="basic_single", size=1024, scenario="StandardTests"),
  dict(name="pf-b8", topology="basic_single", size=2048, scenario="StandardTests"),

  dict(name="pf-m1", topology="basic_multi", size=512, communities=1, degree=4, scenario="StandardTests"),
  dict(name="pf-m2", topology="basic_multi", size=512, communities=2, degree=4, scenario="StandardTests"),
  dict(name="pf-m3", topology="basic_multi", size=512, communities=4, degree=4, scenario="StandardTests"),
  dict(name="pf-m4", topology="basic_multi", size=512, communities=8, degree=4, scenario="StandardTests"),
  dict(name="pf-m5", topology="basic_multi", size=512, communities=16, degree=4, scenario="StandardTests"),

  dict(name="pf-e1", topology="basic_multi", size=256, communities=1, degree=2, scenario="StandardTests"),
  dict(name="pf-e2", topology="basic_multi", size=256, communities=1, degree=4, scenario="StandardTests"),
  dict(name="pf-e3", topology="basic_multi", size=256, communities=1, degree=8, scenario="StandardTests"),
  dict(name="pf-e4", topology="basic_multi", size=256, communities=1, degree=16, scenario="StandardTests"),
  dict(name="pf-e5", topology="basic_multi", size=256, communities=1, degree=32, scenario="StandardTests"),

  # Sybil-tolerance measurement runs
  dict(name="sy-ns1", topology="sybil_count", sybils=16, scenario="SybilNodesNames"),
  dict(name="sy-ns2", topology="sybil_count", sybils=32, scenario="SybilNodesNames"),
  dict(name="sy-ns3", topology="sybil_count", sybils=48, scenario="SybilNodesNames"),
  dict(name="sy-ns4", topology="sybil_count", sybils=64, scenario="SybilNodesNames"),
  dict(name="sy-ns5", topology="sybil_count", sybils=80, scenario="SybilNodesNames"),
  dict(name="sy-ns6", topology="sybil_count", sybils=96, scenario="SybilNodesNames"),

  dict(name="sy-ne1", topology="sybil_edges", attack_edges=60, scenario="SybilNodesNames"),
  dict(name="sy-ne2", topology="sybil_edges", attack_edges=80, scenario="SybilNodesNames"),
  dict(name="sy-ne3", topology="sybil_edges", attack_edges=100, scenario="SybilNodesNames"),
  dict(name="sy-ne4", topology="sybil_edges", attack_edges=120, scenario="SybilNodesNames"),
  dict(name="sy-ne5", topology="sybil_edges", attack_edges=140, scenario="SybilNodesNames"),
  dict(name="sy-ne6", topology="sybil_edges", attack_edges=160, scenario="SybilNodesNames"),
  dict(name="sy-ne7", topology="sybil_edges", attack_edges=180, scenario="SybilNodesNames"),

  dict(name="sy-rs0", topology="sybil_count", sybils=0, scenario="SybilNodesRouting"),
  dict(name="sy-rs1", topology="sybil_count", sybils=16, scenario="SybilNodesRouting"),
  dict(name="sy-rs2", topology="sybil_count", sybils=32, scenario="SybilNodesRouting"),
  dict(name="sy-rs3", topology="sybil_count", sybils=48, scenario="SybilNodesRouting"),
  dict(name="sy-rs4", topology="sybil_count", sybils=64, scenario="SybilNodesRouting"),
  dict(name="sy-rs5", topology="sybil_count", sybils=80, scenario="SybilNodesRouting"),
  dict(name="sy-rs6", topology="sybil_count", sybils=96, scenario="SybilNodesRouting"),

  dict(name="sy-re0", topology="sybil_edges", attack_edges=0, scenario="SybilNodesRouting"),
  dict(name="sy-re1", topology="sybil_edges", attack_edges=5, scenario="SybilNodesRouting"),
  dict(name="sy-re2", topology="sybil_edges", attack_edges=15, scenario="SybilNodesRouting"),
  dict(name="sy-re3", topology="sybil_edges", attack_edges=25, scenario="SybilNodesRouting"),
  dict(name="sy-re4", topology="sybil_edges", attack_edges=30, scenario="SybilNodesRouting"),
  dict(name="sy-re5", topology="sybil_edges", attack_edges=35, scenario="SybilNodesRouting"),
  dict(name="sy-re6", topology="sybil_edges", attack_edges=40, scenario="SybilNodesRouting"),
  dict(name="sy-re7", topology="sybil_edges", attack_edges=45, scenario="SybilNodesRouting"),
  dict(name="sy-re8", topology="sybil_edges", attack_edges=50, scenario="SybilNodesRouting"),
  dict(name="sy-re9", topology="sybil_edges", attack_edges=55, scenario="SybilNodesRouting"),
  dict(name="sy-re10", topology="sybil_edges", attack_edges=60, scenario="SybilNodesRouting"),
]

# Configure graph generation
GRAPHS = [
  # Graphs relating to the effect of increasing sizes on protocol operation
  dict(name="size_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-b*"]),
  dict(name="size_link_congestion", plotter="graphs.LinkCongestion", runs=["pf-b*"]),
  dict(name="size_path_stretch", plotter="graphs.PathStretch", runs=["pf-b*"], variable="size"),
  dict(name="size_state_distribution", plotter="graphs.StateDistribution", runs=["pf-b*"]),
  dict(name="size_ndb_state_vs_size", plotter="graphs.StateVsSize", runs=["pf-b*"], state="ndb_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
  dict(name="size_rt_state_vs_size", plotter="graphs.StateVsSize", runs=["pf-b*"], state="rt_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
  dict(name="size_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="size",
    graph="input-topology.graphml", runs=["pf-b*"]),
  dict(name="size_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="size",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-b*"]),
  dict(name="size_sg_degrees", plotter="graphs.DegreeVsVariable", variable="size",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-b*"]),

  # Graphs relating to the effect of community structure on protocol operation
  dict(name="community_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="communities", legend=False, runs=["pf-m*"]),
  dict(name="community_path_stretch", plotter="graphs.PathStretch", runs=["pf-m*"], variable="communities"),
  dict(name="community_degrees", plotter="graphs.DegreeVsVariable", variable="communities",
    graph="input-topology.graphml", runs=["pf-m*"]),
  dict(name="community_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="communities",
    graph="input-topology.graphml", runs=["pf-m*"]),
  dict(name="community_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="communities",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-m*"]),
  dict(name="community_sg_degrees", plotter="graphs.DegreeVsVariable", variable="communities",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-m*"]),

  # Graphs relating to the effect of mixing time on protocol operation
  dict(name="mixing_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="degree", legend=False, runs=["pf-e*"]),
  dict(name="mixing_path_stretch", plotter="graphs.PathStretch", runs=["pf-e*"], variable="degree"),
  dict(name="mixing_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="degree",
    graph="input-topology.graphml", runs=["pf-e*"]),
  dict(name="mixing_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="degree",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-e*"]),
  dict(name="mixing_sg_degrees", plotter="graphs.DegreeVsVariable", variable="degree",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-e*"]),

  # Graphs relating to the effect of the number of Sybils on protocol operation
  dict(name="sybilnum_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="sybils", legend=False, runs=["sy-ns*"]),

  # Graphs relating to the effect of the number of attack edges on protocol operation
  dict(name="sybiledg_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="attack_edges", legend=False, runs=["sy-ne*"]),

  # Graphs relating to the effect of the number of Sybils on routing
  dict(name="sybilnum_deliverability", plotter="graphs.DeliverabilityVsVariable", runs=["sy-rs*"],
    variable="sybils"),

  # Graphs relating to the effect of the number of attack edges on routing
  dict(name="sybiledg_deliverability", plotter="graphs.DeliverabilityVsAttackEdges", runs=["sy-re*"]),
]

# Configure logging
LOGGING = {
  'version': 1,
  'formatters': {
    'simple': {
      'format': '[%(levelname)s/%(name)s] %(message)s'
    }
  },
  'handlers': {
    'console': {
      'class': 'logging.StreamHandler',
      'formatter': 'simple'
    }
  },
  'loggers': {
    'testbed': {
      'handlers': ['console'],
      'level': 'DEBUG'
    }
  }
}
