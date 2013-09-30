from testbed import graphs

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

# Configure test runs
RUNS = [
  # Performance measurement runs
  dict(name="perf1", topology="symmetric-topo-n16.graphml", size=16, scenario="StandardTests", communities=1),
  dict(name="perf2", topology="symmetric-topo-n32.graphml", size=32, scenario="StandardTests", communities=1),
  dict(name="perf3", topology="symmetric-topo-n64.graphml", size=64, scenario="StandardTests", communities=1),
  dict(name="perf4", topology="symmetric-topo-n128.graphml", size=128, scenario="StandardTests", communities=1),
  dict(name="perf5", topology="symmetric-topo-n256.graphml", size=256, scenario="StandardTests", communities=1),
  dict(name="perf6", topology="symmetric-topo-n512.graphml", size=512, scenario="StandardTests", communities=1),
  dict(name="perf7", topology="symmetric-topo-n1024.graphml", size=1024, scenario="StandardTests", communities=1),

  # Sybil-tolerance measurement runs
  dict(name="sy-s1", topology="sybil_s16_n144.graphml", size=144, scenario="SybilNodes", communities=3, sybils=16),
  dict(name="sy-s2", topology="sybil_s32_n160.graphml", size=160, scenario="SybilNodes", communities=3, sybils=32),
  dict(name="sy-s3", topology="sybil_s48_n176.graphml", size=176, scenario="SybilNodes", communities=3, sybils=48),
  dict(name="sy-s4", topology="sybil_s64_n192.graphml", size=192, scenario="SybilNodes", communities=3, sybils=64),
  dict(name="sy-s5", topology="sybil_s80_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80),
  dict(name="sy-s6", topology="sybil_s96_n224.graphml", size=224, scenario="SybilNodes", communities=3, sybils=96),

  dict(name="sy-e1", topology="sybil_e60_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=60),
  dict(name="sy-e2", topology="sybil_e80_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=80),
  dict(name="sy-e3", topology="sybil_e100_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=100),
  dict(name="sy-e4", topology="sybil_e120_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=120),
  dict(name="sy-e5", topology="sybil_e140_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=140),
  dict(name="sy-e6", topology="sybil_e160_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=160),
  dict(name="sy-e7", topology="sybil_e180_n208.graphml", size=208, scenario="SybilNodes", communities=3, sybils=80, sybil_edges=180),
]

# Configure graph generation
GRAPHS = [
  dict(name="messaging_performance", plotter=graphs.MessagingPerformance, runs=["perf1", "perf2"]),
  dict(name="link_congestion", plotter=graphs.LinkCongestion, runs=["perf1", "perf2"]),
  dict(name="path_stretch", plotter=graphs.PathStretch, runs=["perf1", "perf2"]),
  dict(name="state_distribution", plotter=graphs.StateDistribution, runs=["perf1", "perf2"]),
  dict(name="ndb_state_vs_size", plotter=graphs.StateVsSize, runs=["perf1", "perf2"], state="ndb_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
  dict(name="rt_state_vs_size", plotter=graphs.StateVsSize, runs=["perf1", "perf2"], state="rt_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
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
