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
  dict(name="run1", topology="symmetric-topo-n16.graphml", size=16, scenario="StandardTests", communities=1),
  dict(name="run2", topology="symmetric-topo-n32.graphml", size=32, scenario="StandardTests", communities=1),
]

# Configure graph generation
GRAPHS = [
  dict(name="messaging_performance", plotter=graphs.MessagingPerformance, runs=["run1", "run2"]),
  dict(name="link_congestion", plotter=graphs.LinkCongestion, runs=["run1", "run2"]),
  dict(name="path_stretch", plotter=graphs.PathStretch, runs=["run1", "run2"]),
  dict(name="state_distribution", plotter=graphs.StateDistribution, runs=["run1", "run2"]),
  dict(name="ndb_state_vs_size", plotter=graphs.StateVsSize, runs=["run1", "run2"], state="ndb_s_act",
    fit=lambda x, a, b: a*numpy.sqrt(x)+b, fit_label='Fit of $a \sqrt{x} + c$'),
  dict(name="rt_state_vs_size", plotter=graphs.StateVsSize, runs=["run1", "run2"], state="rt_s_act",
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
