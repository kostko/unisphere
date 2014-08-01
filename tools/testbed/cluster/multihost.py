#
# This file is part of UNISPHERE.
#
# Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
import os
import paramiko
import shutil
import subprocess
import threading
import time


logger = logging.getLogger('testbed.cluster.multihost')

class Host(object):
  """
  Represents a testbed host accessible via an SSH connection.
  """

  def __init__(self, cluster, host):
    """
    Constructs a host object.

    :param cluster: Cluster reference
    :param host: Host configuration identifier
    """

    self.cluster = cluster
    self.host_id = host
    self.host_cfg = self.cluster.cluster_cfg['hosts'][host]
    self.host = paramiko.SSHClient()
    self.host.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    self.host.connect(
      hostname=self.host_cfg['host'],
      username=self.cluster.cluster_cfg['username'],
      key_filename=self.cluster.cluster_cfg['keyfile'],
    )
    self.private_ip = None
    self.failed = False
    self.testbed_nodes = []

    self.obtain_configuration()
    logger.info("Discovered host '%s' with private IP '%s'." % (host, self.private_ip))

    self.sftp = self.host.get_transport().open_sftp_client()

  def obtain_configuration(self):
    """
    Obtains networking configuration from the host.
    """

    _, stdout, _ = self.host.exec_command('ip addr sh dev %s' % self.host_cfg['interface'])
    for line in stdout:
      line = line.strip()
      if line.startswith('inet '):
        self.private_ip = line.split()[1].split('/')[0]
        break
    else:
      raise Exception("Unable to obtain host network configuration for interface '%s'!" % self.host_cfg['interface'])

  def testbed_watcher(self, session, args, logfile):
    """
    Watcher thread entry point.
    """

    try:
      session.set_combine_stderr(True)
      session.exec_command("%s %s" % (
        self.cluster.cluster_cfg['testbed_binary'],
        " ".join(args)
      ))

      log = session.makefile()
      while True:
        buf = log.read(128)
        if not buf:
          break

        if logfile:
          logfile.write(buf)
          logfile.flush()

      if session.recv_exit_status() != 0:
        self.failed = True
    except:
      import traceback
      logger.error("Remote testbed invocation failed with exception:")
      logger.error(traceback.format_exc())
      self.failed = True

  def start_testbed(self, args, logfile=None):
    """
    Executes a testbed instance on the host.

    :param args: Testbed argument list
    :param logfile: Optional logfile
    """

    testbed = threading.Thread(
      target=self.testbed_watcher,
      args=(
        self.host.get_transport().open_session(),
        args,
        logfile
      )
    )
    self.testbed_nodes.append(testbed)
    testbed.start()
    return testbed

  def clear_output_directory(self):
    """
    Clears the remote output directory.
    """

    if not self.cluster.remote_output_directory:
      return

    self.host.exec_command("rm -rf %s/*" % self.cluster.remote_output_directory)

  def copy_output_data(self):
    """
    Copies output data from the host.
    """

    if not self.cluster.remote_output_directory:
      return

    try:
      remote_archive_path = os.path.join(self.cluster.remote_output_directory, 'output.tar.bz2')
      self.host.exec_command("tar --create --bzip2 --file %s --directory %s --exclude output.tar.bz2 ." % (
        remote_archive_path,
        self.cluster.remote_output_directory
      ))
      local_archive_path = os.path.join(self.cluster.local_output_directory, 'output.tar.bz2')
      self.sftp.get(remote_archive_path, local_archive_path)
      self.sftp.remove(remote_archive_path)
      subprocess.call([
        "tar", "--extract", "--file", local_archive_path,
        "--directory", self.cluster.local_output_directory
      ])
      os.remove(local_archive_path)
    except:
      import traceback
      logger.error("Failed to copy output data:")
      logger.error(traceback.format_exc())

  def close(self):
    """
    Closes connection with the host and terminates all running testbed
    instances.
    """

    self.host.exec_command("killall -9 testbed")
    self.host.close()

    for node in self.testbed_nodes:
      node.join()

class MultihostCluster(base.ClusterRunnerBase):

  local_output_directory = None
  remote_output_directory = None

  host_mc = None
  host_workers = []

  slaves = []

  def setup(self, run, run_id):
    try:
      # First establish all connections
      self.host_mc = Host(self, 'mc')
      self.host_workers = []
      for cfg_id in self.cluster_cfg['hosts']:
        if cfg_id == 'mc':
          continue

        self.host_workers.append(Host(self, cfg_id))

      # Generate identifier for the cluster master
      self.master_private_key = 'W3qqkUybqur79JJbxIiWYcayXgt+tiWF6D+5T7/HS8YfbBlLHGqK2KtEwqtBEO/a4Lx7XPcXZKUQthvByC2x09NRG1icM43SnlDnFOy3eV0jUTPnJwBACh2tENJOrI6r'
      self.master_public_key = 'H2wZSxxqitirRMKrQRDv2uC8e1z3F2SlELYbwcgtsdOj57Ei5oZ3fDnD+HY9TQQPOQckN6P1fF38VlnAlbfBPA=='

      # Prepare output directory
      out_dir = os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name)
      if os.path.exists(out_dir):
        logger.warning("Removing previous output directory '%s'..." % run.name)
        shutil.rmtree(out_dir)

      os.makedirs(out_dir)
      self.local_output_directory = out_dir

      # Store run identifier
      with open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "version"), 'w') as f:
        f.write("%s\n" % run_id)

      # Open files for logging execution
      logger.info("Opening log files to monitor scenario execution...")
      self.log_master = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_master.log"), 'w')
      self.log_controller = open(os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, "tb_controller.log"), 'w')

      # Start cluster master process
      logger.info("Starting master node...")
      self.host_mc.start_testbed(
        [
          "--cluster-role", "master",
          "--cluster-ip", self.host_mc.private_ip,
          "--cluster-port", "2001",
          "--cluster-priv-key", self.master_private_key,
          "--cluster-pub-key", self.master_public_key,
          "--dataset-storage", 'mongodb://%s:27017/' % self.host_mc.private_ip,
        ],
        logfile=self.log_master,
      )

      # Start cluster slave process(es)
      self.slaves = []
      control_port = 2003
      for worker in self.host_workers:
        port_start = 3072
        ports_per_slave = 512
        port_end = port_start + (ports_per_slave * worker.host_cfg['workers'])

        logger.info("Starting %d slave node(s) on host '%s' (ports %d-%d)..." % (
          worker.host_cfg['workers'],
          worker.host_id,
          port_start,
          port_end,
        ))

        for slave_id in xrange(1, worker.host_cfg['workers'] + 1):
          logger.info("  * Starting slave %d (ports %d-%d)." % (slave_id, port_start, port_start + ports_per_slave - 1))
          log_slave = None
          worker.start_testbed(
            [
              "--cluster-role", "slave",
              "--cluster-ip", worker.private_ip,
              "--cluster-port", str(control_port),
              "--cluster-master-ip", self.host_mc.private_ip,
              "--cluster-master-port", "2001",
              "--cluster-master-pub-key", self.master_public_key,
              "--sim-ip", worker.private_ip,
              "--sim-port-start", str(port_start),
              "--sim-port-end", str(port_start + ports_per_slave - 1),
              "--sim-threads", "1",
              "--exit-on-finish",
              "--log-disable",
            ],
            logfile=log_slave
          )
          self.slaves.append(dict(log=log_slave))
          port_start += ports_per_slave
          control_port += 1

      # Wait for the slaves to register themselves
      time.sleep(5)

      logger.info("Cluster ready.")
    except:
      logger.error("Error while setting up cluster, aborting!")
      raise

  def run_scenario(self, run, run_id):
    if self.host_mc is None or any([s.failed for s in self.host_workers]):
      logger.error("Cluster not ready, aborting scenario run.")
      raise exceptions.ScenarioRunFailed

    # Generate required topology
    topology_path = os.path.join(self.settings.OUTPUT_DIRECTORY, run_id, run.name, 'input-topology.graphml')
    run.generate_topology(topology_path)

    # Prepare remote output directory
    remote_output_directory = os.path.join(self.cluster_cfg['testbed_output'], run_id, run.name)
    self.host_mc.sftp.mkdir(os.path.join(self.cluster_cfg['testbed_output'], run_id))
    self.host_mc.sftp.mkdir(os.path.join(self.cluster_cfg['testbed_output'], run_id, run.name))
    self.remote_output_directory = remote_output_directory

    self.host_mc.clear_output_directory()

    # Upload topology to MC host
    remote_topology_path = os.path.join(remote_output_directory, 'input-topology.graphml')
    self.host_mc.sftp.put(topology_path, remote_topology_path)

    # Run the scenario via the controller
    controller = self.host_mc.start_testbed(
      [
        "--cluster-role", "controller",
        "--cluster-ip", self.host_mc.private_ip,
        "--cluster-port", "2002",
        "--cluster-master-ip", self.host_mc.private_ip,
        "--cluster-master-port", "2001",
        "--cluster-master-pub-key", self.master_public_key,
        "--topology", remote_topology_path,
        "--scenario", run.settings['scenario'],
        "--out-dir", remote_output_directory,
        "--id-gen", run.settings.get('id_gen', 'consistent'),
        "--seed", str(run.settings.get('seed', 1)),
      ],
      logfile=self.log_controller,
    )

    # Wait for the scenario to complete; this loop is used instead of a join
    # in order to handle keyboard interrupts
    while controller.isAlive():
      time.sleep(1)
    if self.host_mc.failed:
      raise exceptions.ScenarioRunFailed

    logger.info("Simulation finished, stand by...")
    time.sleep(15)

  def shutdown(self):
    logger.info("Shutting down cluster...")

    if self.host_mc:
      logger.info("Copying data from MC node...")
      self.host_mc.copy_output_data()
      self.host_mc.close()
    for host in self.host_workers:
      host.close()

    self.log_master.close()
    self.log_controller.close()

    for slave in self.slaves:
      if slave['log']:
        slave['log'].close()

    self.local_output_directory = None
    self.remote_output_directory = None
    self.host_mc = None
    self.host_workers = []
    self.slaves = []
