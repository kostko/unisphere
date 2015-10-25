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

from . import base
from .. import exceptions

import logging
import math
import multiprocessing
import os
import resource
import shutil
import subprocess
import time

logger = logging.getLogger('testbed.cluster.simple')


class SimpleCluster(base.ClusterRunnerBase):
  """
  Sets up a simple cluster running on a single machine.
  """

  master_id = None

  master = None
  slaves = None
  controller = None

  log_master = None
  log_controller = None

  def _setup_limits(self):
    # Enable the production of core dumps
    resource.setrlimit(resource.RLIMIT_CORE, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))

  def setup(self, run, run_id):
    """
    Prepares the cluster for running the emulation.
    """

    try:
      # Generate identifier for the cluster master
      self.master_private_key = 'W3qqkUybqur79JJbxIiWYcayXgt+tiWF6D+5T7/HS8YfbBlLHGqK2KtEwqtBEO/a4Lx7XPcXZKUQthvByC2x09NRG1icM43SnlDnFOy3eV0jUTPnJwBACh2tENJOrI6r'
      self.master_public_key = 'H2wZSxxqitirRMKrQRDv2uC8e1z3F2SlELYbwcgtsdOj57Ei5oZ3fDnD+HY9TQQPOQckN6P1fF38VlnAlbfBPA=='

      # Prepare output directory
      out_dir = os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name)
      if os.path.exists(out_dir):
        logger.warning("Removing previous output directory '%s'..." % run.name)
        shutil.rmtree(out_dir)

      os.makedirs(out_dir)

      # Store run identifier
      with open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "version"), 'w') as f:
        f.write("%s\n" % run_id)

      # Open files for logging execution
      logger.info("Opening log files to monitor scenario execution...")
      self.log_master = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_master.log"), 'w')
      self.log_controller = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_controller.log"), 'w')

      # Start cluster master process
      logger.info("Starting master node...")
      self.master = subprocess.Popen(
        [
          self.settings.TESTBED_BINARY,
          "--dataset-storage", self.cluster_cfg['dataset_storage'],
          "--cluster-role", "master",
          "--cluster-ip", self.cluster_cfg['master_ip'],
          "--cluster-priv-key", self.master_private_key,
          "--cluster-pub-key", self.master_public_key
        ],
        stdin=None,
        stderr=self.log_master,
        stdout=self.log_master,
        # Ensure proper working directory
        cwd=self.settings.TESTBED_ROOT,
        # Ensure that resource limits are configured correctly before starting
        preexec_fn=self._setup_limits
      )

      # Compute how we will distribute the slaves around; if there are more than 2 cores
      # then we use multiple slaves with 2 threads each; this is because with too many
      # threads, Boost.ASIO queue lock contention becomes too big
      required_slaves = int(math.ceil(
        float(multiprocessing.cpu_count()) / self.cluster_cfg.get('threads_per_slave', 2)
      ))

      # Start cluster slave process(es)
      logger.info("Starting %d slave node(s)..." % required_slaves)
      self.slaves = []
      for slave_id in xrange(1, required_slaves + 1):
        logger.info("  * Starting slave %d." % slave_id)
        log_slave = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_slave%d.log" % slave_id), 'w')
        slave = subprocess.Popen(
          [
            self.settings.TESTBED_BINARY,
            "--cluster-role", "slave",
            "--cluster-ip", self.cluster_cfg['slave_ip'] % slave_id,
            "--cluster-master-ip", self.cluster_cfg['master_ip'],
            "--cluster-master-pub-key", self.master_public_key,
            "--sim-ip", self.cluster_cfg['slave_sim_ip'] % slave_id,
            "--sim-port-start", str(self.cluster_cfg['slave_sim_ports'][0]),
            "--sim-port-end", str(self.cluster_cfg['slave_sim_ports'][1]),
            "--sim-threads", str(self.cluster_cfg.get('threads_per_slave', 2)),
            "--exit-on-finish"
          ],
          stdin=None,
          stderr=log_slave,
          stdout=log_slave,
          # Ensure proper working directory
          cwd=self.settings.TESTBED_ROOT,
          # Ensure that resource limits are configured correctly before starting
          preexec_fn=self._setup_limits
        )
        self.slaves.append(dict(slave=slave, log=log_slave))

      # Wait for the slaves to register themselves
      time.sleep(5)

      logger.info("Simple cluster ready.")
    except:
      logger.error("Error while setting up cluster, aborting!")
      raise

  def run_scenario(self, run, run_id):
    """
    Starts the controller and begins the emulation.
    """

    if self.master is None or not self.slaves:
      logger.error("Cluster not ready, aborting scenario run.")
      raise exceptions.ScenarioRunFailed

    # Generate required topology
    topology_path = os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, 'input-topology.graphml')
    run.generate_topology(topology_path)

    # Run the scenario via the controller
    self.controller = subprocess.Popen(
      [
        self.settings.TESTBED_BINARY,
        "--cluster-role", "controller",
        "--cluster-ip", self.cluster_cfg['controller_ip'],
        "--cluster-master-ip", self.cluster_cfg['master_ip'],
        "--cluster-master-pub-key", self.master_public_key,
        "--topology", topology_path,
        "--scenario", run.settings['scenario'],
        "--out-dir", os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name),
        "--id-gen", run.settings.get('id_gen', 'consistent'),
        "--seed", str(run.settings.get('seed', 1))
      ],
      stdin=None,
      stderr=self.log_controller,
      stdout=self.log_controller,
      # Ensure proper working directory
      cwd=self.settings.TESTBED_ROOT,
      # Ensure that resource limits are configured correctly before starting
      preexec_fn=self._setup_limits
    )

    # Wait for the scenario to complete
    if self.controller.wait() != 0:
      raise exceptions.ScenarioRunFailed

    time.sleep(5)

  def shutdown(self):
    """
    Shuts down the testbed environment.
    """

    logger.info("Shutting down cluster...")

    # Terminate everything
    for process in (self.controller, self.master):
      if process is None:
        continue

      try:
        process.kill()
        process.wait()
      except OSError:
        pass

    for slave in (self.slaves or []):
      try:
        slave['slave'].kill()
        slave['slave'].wait()
      except OSError:
        pass

      slave['log'].close()

    # Reset
    self.controller = None
    self.slaves = None
    self.master = None
    self.master_id = None

    self.log_master.close()
    self.log_controller.close()
