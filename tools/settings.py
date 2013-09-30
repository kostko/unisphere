from testbed import graphs, topologies

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
    'slave_ip': '127.0.0.3',
    'slave_sim_ip': '127.0.1.1',
    'slave_sim_ports': (9000, 20000),
    'slave_sim_threads': 5
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
      dict(src="sybil", dst="honest", count=lambda a: 10*int(math.log(a.nodes))),
      dict(src="sybil", dst="foreign", count=lambda a: 10*int(math.log(a.nodes))),
      # Edges to foreign community
      dict(src="honest", dst="foreign", count=lambda a: int(math.log(a.nodes))**2),
    ],
  ),

  # Varying number of attack edges, but number of all communities stay the same
  dict(
    name="sybil_edges",
    args=['attack_edges'],
    communities=dict(
      honest=dict(n=64, degree=4, rewire=0.8),
      sybil=dict(n=80, degree=4, rewire=0.8),
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

  # Sybil-tolerance measurement runs
  dict(name="sy-s1", topology="sybil_count", sybils=16, scenario="SybilNodes"),
  dict(name="sy-s2", topology="sybil_count", sybils=32, scenario="SybilNodes"),
  dict(name="sy-s3", topology="sybil_count", sybils=48, scenario="SybilNodes"),
  dict(name="sy-s4", topology="sybil_count", sybils=64, scenario="SybilNodes"),
  dict(name="sy-s5", topology="sybil_count", sybils=80, scenario="SybilNodes"),
  dict(name="sy-s6", topology="sybil_count", sybils=96, scenario="SybilNodes"),

  dict(name="sy-e1", topology="sybil_edges", attack_edges=60, scenario="SybilNodes"),
  dict(name="sy-e2", topology="sybil_edges", attack_edges=80, scenario="SybilNodes"),
  dict(name="sy-e3", topology="sybil_edges", attack_edges=100, scenario="SybilNodes"),
  dict(name="sy-e4", topology="sybil_edges", attack_edges=120, scenario="SybilNodes"),
  dict(name="sy-e5", topology="sybil_edges", attack_edges=140, scenario="SybilNodes"),
  dict(name="sy-e6", topology="sybil_edges", attack_edges=160, scenario="SybilNodes"),
  dict(name="sy-e7", topology="sybil_edges", attack_edges=180, scenario="SybilNodes"),
]

# Configure graph generation
GRAPHS = [
  dict(name="messaging_performance", plotter=graphs.MessagingPerformance, runs=["pf-b1", "pf-b2"]),
  dict(name="link_congestion", plotter=graphs.LinkCongestion, runs=["pf-b1", "pf-b2"]),
  dict(name="path_stretch", plotter=graphs.PathStretch, runs=["pf-b1", "pf-b2"]),
  dict(name="state_distribution", plotter=graphs.StateDistribution, runs=["pf-b1", "pf-b2"]),
  dict(name="ndb_state_vs_size", plotter=graphs.StateVsSize, runs=["pf-b1", "pf-b2"], state="ndb_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
  dict(name="rt_state_vs_size", plotter=graphs.StateVsSize, runs=["pf-b1", "pf-b2"], state="rt_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),

  dict(name="sybil_msg_perf", plotter=graphs.MessagingPerformance,
    label_attribute="attack_edges", legend=False,
    runs=["sy-e1", "sy-e2", "sy-e3", "sy-e4", "sy-e5", "sy-e6", "sy-e7"]),
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
