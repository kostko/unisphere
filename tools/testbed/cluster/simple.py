#
# This file is part of UNISPHERE.
#
# Copyright (C) 2013 Jernej Kos <k@jst.sm>
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

import hashlib
import logging
import os
import resource
import shutil
import subprocess
import time

logger = logging.getLogger('testbed.cluster.simple')

class SimpleCluster(base.ClusterRunnerBase):
  master_id = None

  master = None
  slave = None
  controller = None

  log_slave = None
  log_master = None
  log_controller = None

  def _setup_limits(self):
    # Enable the production of core dumps
    resource.setrlimit(resource.RLIMIT_CORE, (resource.RLIM_INFINITY, resource.RLIM_INFINITY))

  def setup(self, run, run_id):
    try:
      # Generate identifier for the cluster master
      self.master_id = hashlib.sha1(os.urandom(32)).hexdigest()

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
      self.log_slave = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_slave.log"), 'w')
      self.log_master = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_master.log"), 'w')
      self.log_controller = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_controller.log"), 'w')

      # Start cluster master process
      logger.info("Starting master node...")
      self.master = subprocess.Popen(
        [
          self.settings.TESTBED_BINARY,
          "--cluster-role", "master",
          "--cluster-ip", self.cluster_cfg['master_ip'],
          "--cluster-node-id", self.master_id
        ],
        stdin=None,
        stderr=self.log_master,
        stdout=self.log_master,
        # Ensure proper working directory
        cwd=self.settings.TESTBED_ROOT,
        # Ensure that resource limits are configured correctly before starting
        preexec_fn=self._setup_limits
      )

      # Start cluster slave process
      logger.info("Starting slave node...")
      self.slave = subprocess.Popen(
        [
          self.settings.TESTBED_BINARY,
          "--cluster-role", "slave",
          "--cluster-ip", self.cluster_cfg['slave_ip'],
          "--cluster-master-ip", self.cluster_cfg['master_ip'],
          "--cluster-master-id", self.master_id,
          "--sim-ip", self.cluster_cfg['slave_sim_ip'],
          "--sim-port-start", str(self.cluster_cfg['slave_sim_ports'][0]),
          "--sim-port-end", str(self.cluster_cfg['slave_sim_ports'][1]),
          "--sim-threads", str(self.cluster_cfg['slave_sim_threads'])
        ],
        stdin=None,
        stderr=self.log_slave,
        stdout=self.log_slave,
        # Ensure proper working directory
        cwd=self.settings.TESTBED_ROOT,
        # Ensure that resource limits are configured correctly before starting
        preexec_fn=self._setup_limits
      )

      # Wait for the slave to register itself
      time.sleep(5)
      
      logger.info("Simple cluster ready.")
    except:
      logger.error("Error while setting up cluster, aborting!")
      raise

  def run_scenario(self, run, run_id):
    if self.master is None or self.slave is None:
      logger.error("Cluster not ready, aborting scenario run.")
      raise exceptions.ScenarioRunFailed

    # Run the scenario via the controller
    self.controller = subprocess.Popen(
      [
        self.settings.TESTBED_BINARY,
        "--cluster-role", "controller",
        "--cluster-ip", self.cluster_cfg['controller_ip'],
        "--cluster-master-ip", self.cluster_cfg['master_ip'],
        "--cluster-master-id", self.master_id,
        "--topology", os.path.join(self.settings.DATA_DIRECTORY, run.settings['topology']),
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

  def shutdown(self):
    logger.info("Shutting down cluster...")

    # Terminate everything
    for process in (self.controller, self.slave, self.master):
      if process is None:
        continue

      try:
        process.kill()
        process.wait()
      except OSError:
        pass

    # Reset
    self.controller = None
    self.slave = None
    self.master = None
    self.master_id = None

    self.log_slave.close()
    self.log_master.close()
    self.log_controller.close()
