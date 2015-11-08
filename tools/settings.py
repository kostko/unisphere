# -*- coding: utf-8 -*-
import math
import numpy
import os

TESTBED_ROOT = os.path.realpath(os.path.join(os.path.abspath(os.path.dirname(__file__)), ".."))
TESTBED_BINARY = os.path.join(TESTBED_ROOT, "build/release/apps/testbed/testbed")
DATA_DIRECTORY = os.path.join(TESTBED_ROOT, "data")
OUTPUT_DIRECTORY = os.path.join(TESTBED_ROOT, "output")
OUTPUT_GRAPH_FORMAT = "pdf"
GRAPH_TRANSPARENCY = False

# Cluster configuration. Each key defines a Python module which will be responsible
# for setting up the cluster.
CLUSTERS = {
  #
  # Simple cluster runs all testbed components on a single machine. This should
  # only be used to simulate small topologies.
  #
  'testbed.cluster.simple.SimpleCluster': {
    'master_ip': '127.0.0.1',
    'controller_ip': '127.0.0.2',
    'slave_ip': '127.0.1.%d',
    'slave_sim_ip': '127.0.2.%d',
    'slave_sim_ports': (9000, 20000),
    'dataset_storage': 'mongodb://db/',
  },

  #
  # Multi-host cluster splits master, controller and workers to different machines
  # or virtual instances (for example, running on Amazon EC2). The example configuration
  # below contains configuration for an EC2 setup. You need to change this.
  #
  'testbed.cluster.multihost.MultihostCluster': {
    'username': 'ubuntu',
    'keyfile': '/home/kostko/Keys/keypair-aws-ec2.pem',
    'testbed_root': '/unisphere',
    'testbed_output': '/unisphere/output',
    'testbed_binary': '/unisphere/build/release/apps/testbed/testbed',
    'halt_after_finished': False,
    'hosts': {
      'mc': {'host': '54.213.37.229', 'interface': 'eth0'},
      'w0': {'host': '54.187.202.213', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w1': {'host': '54.191.194.168', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w2': {'host': '54.200.4.122', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w3': {'host': '54.200.193.182', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w4': {'host': '54.201.12.254', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w5': {'host': '54.200.235.102', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w6': {'host': '54.200.225.70', 'interface': 'eth0', 'workers': 30, 'threads': 2},
      'w7': {'host': '54.187.77.156', 'interface': 'eth0', 'workers': 30, 'threads': 2},
    }
  }
}

# Cluster configuration that should be used by default. Should be one of the above keys.
CLUSTER = 'testbed.cluster.simple.SimpleCluster'

# Configure topologies.
TOPOLOGIES = [
  # CJDNS/Hyperboria topology.
  dict(
    generator="topologies.loader.load",
    name="hyperboria",
    filename=os.path.join(DATA_DIRECTORY, "hyperboria-topology.gml"),
    format="gml",
  ),

  # AS-733 #1 topology.
  dict(
    generator="topologies.loader.load",
    name="as-733-a",
    filename=os.path.join(DATA_DIRECTORY, "as19971108.edges"),
    format="edges",
  ),

  # AS-733 #2 topology.
  dict(
    generator="topologies.loader.load",
    name="as-733-b",
    filename=os.path.join(DATA_DIRECTORY, "as20000102.edges"),
    format="edges",
  ),

  # SNAP Facebook topology.
  dict(
    generator="topologies.loader.load",
    name="facebook",
    filename=os.path.join(DATA_DIRECTORY, "facebook-social-circles-topology.edges"),
    format="edges",
  ),

  # A single community but a custom number of nodes.
  dict(
    generator="topologies.generator.generate",
    name="basic_single",
    args=['size'],
    communities=dict(
      honest=dict(type='holme-kim', n=lambda a: a.size, degree=4, rewire=0.2),
    ),
  ),

  # A fixed number of nodes, but multiple communities.
  dict(
    generator="topologies.generator.generate",
    name="basic_multi",
    args=['size', 'communities', 'degree'],
    communities=lambda a: dict([
      ("c%d" % i, dict(type='holme-kim', n=a.size // a.communities, degree=a.degree, rewire=0.2))
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
  # communities stay the same.
  dict(
    generator="topologies.generator.generate",
    name="sybil_count",
    args=['sybils'],
    communities=dict(
      honest=dict(type='holme-kim', n=64, degree=4, rewire=0.2),
      sybil=dict(type='holme-kim', n=lambda a: a.sybils, degree=4, rewire=0.2),
      foreign=dict(type='holme-kim', n=64, degree=4, rewire=0.2),
    ),
    connections=[
      # Attack edges
      dict(src="sybil", dst="honest", count=lambda a: int(math.log(a.nodes))),
      dict(src="sybil", dst="foreign", count=lambda a: int(math.log(a.nodes))),
      # Edges to foreign community
      dict(src="honest", dst="foreign", count=lambda a: int(math.log(a.nodes))),
    ],
  ),

  # Varying number of attack edges, but size of all communities stays the same.
  dict(
    generator="topologies.generator.generate",
    name="sybil_edges",
    args=['attack_edges'],
    communities=dict(
      honest=dict(type='holme-kim', n=64, degree=4, rewire=0.2),
      sybil=dict(type='holme-kim', n=64, degree=4, rewire=0.2),
      foreign=dict(type='holme-kim', n=64, degree=4, rewire=0.2),
    ),
    connections=[
      # Attack edges.
      dict(src="sybil", dst="honest", count=lambda a: a.attack_edges),
      dict(src="sybil", dst="foreign", count=lambda a: a.attack_edges),
      # Edges to foreign community.
      dict(src="honest", dst="foreign", count=25),
    ],
  ),
]

# Configure test runs.
RUNS = [
  # Performance measurement runs.
  dict(name="pf-b1", topology="basic_single", size=16, scenario="StandardTests"),
  dict(name="pf-b2", topology="basic_single", size=32, scenario="StandardTests"),
  dict(name="pf-b3", topology="basic_single", size=64, scenario="StandardTests"),
  dict(name="pf-b4", topology="basic_single", size=128, scenario="StandardTests"),
  dict(name="pf-b5", topology="basic_single", size=256, scenario="StandardTests"),
  dict(name="pf-b6", topology="basic_single", size=512, scenario="StandardTests"),
  dict(name="pf-b7", topology="basic_single", size=1024, scenario="StandardTests"),
  dict(name="pf-b8", topology="basic_single", size=2048, scenario="StandardTests"),
  dict(name="pf-b9", topology="basic_single", size=4096, scenario="StandardTests"),

  dict(name="pf-hb", topology="hyperboria", scenario="StandardTests"),

  dict(name="pf-as733-a", topology="as-733-a", scenario="StandardTests"),
  dict(name="pf-as733-b", topology="as-733-b", scenario="StandardTests"),

  dict(name="pf-fb", topology="facebook", scenario="StandardTests"),

  dict(name="pf-e1", topology="basic_multi", size=256, communities=1, degree=2, scenario="StandardTests"),
  dict(name="pf-e2", topology="basic_multi", size=256, communities=1, degree=4, scenario="StandardTests"),
  dict(name="pf-e3", topology="basic_multi", size=256, communities=1, degree=8, scenario="StandardTests"),
  dict(name="pf-e4", topology="basic_multi", size=256, communities=1, degree=16, scenario="StandardTests"),
  dict(name="pf-e5", topology="basic_multi", size=256, communities=1, degree=32, scenario="StandardTests"),

  dict(name="pf-m1", topology="basic_multi", size=512, communities=1, degree=4, scenario="StandardTests"),
  dict(name="pf-m2", topology="basic_multi", size=512, communities=2, degree=4, scenario="StandardTests"),
  dict(name="pf-m3", topology="basic_multi", size=512, communities=4, degree=4, scenario="StandardTests"),
  dict(name="pf-m4", topology="basic_multi", size=512, communities=8, degree=4, scenario="StandardTests"),
  dict(name="pf-m5", topology="basic_multi", size=512, communities=16, degree=4, scenario="StandardTests"),

  # Scenarios with churn.
  dict(name="pf-ch1", topology="basic_multi", size=256, communities=1, degree=16, scenario="Churn"),

  # Long-running performance test.
  dict(name="pf-long1", topology="basic_single", size=256, scenario="LongRunningPerformanceTest"),

  # Repeat all attack edge routing runs 10 times.
  dict(apply_to=["sy-re*"], repeats=10),

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

  dict(name="sy-ne0", topology="sybil_edges", attack_edges=0, scenario="SybilNodesNames"),
  dict(name="sy-ne1", topology="sybil_edges", attack_edges=5, scenario="SybilNodesNames"),
  dict(name="sy-ne2", topology="sybil_edges", attack_edges=15, scenario="SybilNodesNames"),
  dict(name="sy-ne3", topology="sybil_edges", attack_edges=25, scenario="SybilNodesNames"),
  dict(name="sy-ne4", topology="sybil_edges", attack_edges=30, scenario="SybilNodesNames"),
  dict(name="sy-ne5", topology="sybil_edges", attack_edges=35, scenario="SybilNodesNames"),
  dict(name="sy-ne6", topology="sybil_edges", attack_edges=40, scenario="SybilNodesNames"),
  dict(name="sy-ne7", topology="sybil_edges", attack_edges=45, scenario="SybilNodesNames"),
  dict(name="sy-ne8", topology="sybil_edges", attack_edges=50, scenario="SybilNodesNames"),
  dict(name="sy-ne9", topology="sybil_edges", attack_edges=55, scenario="SybilNodesNames"),
  dict(name="sy-ne10", topology="sybil_edges", attack_edges=60, scenario="SybilNodesNames"),

  dict(name="sy-ld0", topology="sybil_edges", attack_edges=0, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld1", topology="sybil_edges", attack_edges=5, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld2", topology="sybil_edges", attack_edges=15, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld3", topology="sybil_edges", attack_edges=25, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld4", topology="sybil_edges", attack_edges=30, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld5", topology="sybil_edges", attack_edges=35, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld6", topology="sybil_edges", attack_edges=40, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld7", topology="sybil_edges", attack_edges=45, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld8", topology="sybil_edges", attack_edges=50, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld9", topology="sybil_edges", attack_edges=55, scenario="SybilNodesNamesLandmarks"),
  dict(name="sy-ld10", topology="sybil_edges", attack_edges=60, scenario="SybilNodesNamesLandmarks"),
]

# Configure graph generation.
GRAPHS = [
  # Graphs relating to the effect of increasing sizes on protocol operation.
  dict(name="size_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-b*"]),
  dict(name="size_msg_perf_s64", plotter="graphs.MessagingPerformance", runs=["pf-b3"]),
  dict(name="size_msg_perf_s128", plotter="graphs.MessagingPerformance", runs=["pf-b4"]),
  dict(name="size_msg_perf_s256", plotter="graphs.MessagingPerformance", runs=["pf-b5"]),
  dict(name="size_msg_perf_s512", plotter="graphs.MessagingPerformance", runs=["pf-b6"]),
  dict(name="size_msg_perf_s1024", plotter="graphs.MessagingPerformance", runs=["pf-b7"],
    legend_loc='upper left'),
  dict(name="size_msg_perf_s2048", plotter="graphs.MessagingPerformance", runs=["pf-b8"]),
  dict(name="size_msg_perf_s4096", plotter="graphs.MessagingPerformance", runs=["pf-b9"]),
  dict(name="size_link_congestion", plotter="graphs.LinkCongestion", runs=["pf-b8"]),
  dict(name="size_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-b*"], variable="size"),
  dict(name="size_path_stretches", plotter="graphs.PathStretchVsVariable", runs=["pf-b*"],
    variable="size", variable_label=u"Število vozlišč", scale="log"),
  dict(name="size_state_distribution", plotter="graphs.StateDistribution", runs=["pf-b*"]),
  dict(name="size_ndb_state_vs_size", plotter="graphs.StateVsSize", runs=["pf-b*"], state="ndb_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Prileganje kvadratnega korena'),
  dict(name="size_rt_state_vs_size", plotter="graphs.StateVsSize", runs=["pf-b*"], state="rt_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Prileganje kvadratnega korena'),
  dict(name="size_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="size",
    graph="input-topology.graphml", runs=["pf-b*"]),
  dict(name="size_degrees", plotter="graphs.DegreeVsVariable", variable="size", scale="log",
    graph="input-topology.graphml", runs=["pf-b*"]),
  dict(name="size_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="size",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-b*"]),
  dict(name="size_sg_degrees", plotter="graphs.DegreeVsVariable", variable="size", scale="log",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-b*"]),
  dict(name="size_lr_length_dist", plotter="graphs.LRLengthDistribution", runs=["pf-b*"]),
  dict(name="size_lr_lengths", plotter="graphs.LRLengthVsVariable", runs=["pf-b*"], scale="log",
    variable_label=u"Število vozlišč"),

  dict(name="hyperboria_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-hb"]),
  dict(name="hyperboria_link_congestion", plotter="graphs.LinkCongestion", runs=["pf-hb"]),
  dict(name="hyperboria_lr_length_dist", plotter="graphs.LRLengthDistribution", runs=["pf-hb"]),
  dict(name="hyperboria_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-hb"]),
  dict(name="hyperboria_state_distribution", plotter="graphs.StateDistribution", runs=["pf-hb"]),

  dict(name="as733a_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-as733-a"]),
  dict(name="as733a_link_congestion", plotter="graphs.LinkCongestion", runs=["pf-as733-a"]),
  dict(name="as733a_lr_length_dist", plotter="graphs.LRLengthDistribution", runs=["pf-as733-a"]),
  dict(name="as733a_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-as733-a"]),
  dict(name="as733a_state_distribution", plotter="graphs.StateDistribution", runs=["pf-as733-a"]),

  dict(name="as733b_link_congestion", plotter="graphs.LinkCongestion", runs=["pf-as733-b"]),
  dict(name="as733b_lr_length_dist", plotter="graphs.LRLengthDistribution", runs=["pf-as733-b"]),
  dict(name="as733b_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-as733-b"]),
  dict(name="as733b_state_distribution", plotter="graphs.StateDistribution", runs=["pf-as733-b"]),

  dict(
    name="overall_path_stretch_dist",
    plotter="graphs.OverallPathStretchDistribution",
    runs=["pf-b2", "pf-b3", "pf-b4", "pf-b5", "pf-b6", "pf-b7", "pf-b8", "pf-b9", "pf-hb", "pf-as733-*"],
    topologies=[
      {'topology': ('basic_single',), 'colormap': 'BuPu', 'legend': 'synthetic-hk (n=%s)', 'variable': 'size'},
      {'topology': ('hyperboria',), 'color': '#6E9B34', 'legend': 'hyperboria'},
      {'topology': ('as-733-a',), 'color': '#AAA639', 'legend': 'as-733-a'},
      {'topology': ('as-733-b',), 'color': '#D1CD62', 'legend': 'as-733-b'},
    ],
  ),

  dict(name="facebook_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-fb"]),

  # Graphs relating to the effect of mixing time on protocol operation.
  dict(name="mixing_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="degree", legend=False, runs=["pf-e*"]),
  dict(name="mixing_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-e*"],
    variable="degree", variable_label=u"stopnja"),
  dict(name="mixing_path_stretches", plotter="graphs.PathStretchVsVariable", runs=["pf-e*"], variable="degree",
    variable_label=u"Povprečna stopnja vozlišča"),
  dict(name="mixing_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="degree",
    graph="input-topology.graphml", runs=["pf-e*"]),
  dict(name="mixing_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="degree",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-e*"]),
  dict(name="mixing_sg_degrees", plotter="graphs.DegreeVsVariable", variable="degree",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-e*"]),

  # Graphs relating to the effect of community structure on protocol operation.
  dict(name="community_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="communities", legend=False, runs=["pf-m*"]),
  dict(name="community_path_stretch_dist", plotter="graphs.PathStretchDistribution", runs=["pf-m*"],
    variable="communities", variable_label=u"skupnosti"),
  dict(name="community_path_stretches", plotter="graphs.PathStretchVsVariable", runs=["pf-m*"],
    variable="communities", variable_label=u"Število skupnosti"),
  dict(name="community_degrees", plotter="graphs.DegreeVsVariable", variable="communities",
    graph="input-topology.graphml", runs=["pf-m*"]),
  dict(name="community_rt_degree_dist", plotter="graphs.DegreeDistribution", variable="communities",
    graph="input-topology.graphml", runs=["pf-m*"]),
  dict(name="community_sg_degree_dist", plotter="graphs.DegreeDistribution", variable="communities",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-m*"]),
  dict(name="community_sg_degrees", plotter="graphs.DegreeVsVariable", variable="communities",
    graph="state-sloppy_group_topology-sg-topo-*.graphml", runs=["pf-m*"]),

  # Graphs relating to churn scenarios.
  dict(name="churn_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-ch1"],
    legend_loc='lower right'),

  # Graphs for long-running performance tests.
  dict(name="long_running_msg_perf", plotter="graphs.MessagingPerformance", runs=["pf-long1"]),

  # Graphs relating to the effect of the number of attack edges on protocol operation.
  dict(name="sybiledg_msg_perf", plotter="graphs.MessagingPerformance",
    label_attribute="attack_edges", legend=False, runs=["sy-ne*"]),
  dict(name="sybiledg_availability", plotter="graphs.AvailabilityVsAttackEdges", runs=["sy-re*"]),

  dict(name="sybil_scenarios", plotter="graphs.SybilScenarios", runs=["sy-re*", "sy-ne*", "sy-ld*"]),
]

# Configure logging.
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
