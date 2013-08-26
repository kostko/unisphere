#!/bin/bash
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

. tools/config.sh

echo ">>> Setting up ulimits..."

# Allow core dumps
ulimit -c unlimited
# Increase open file descriptor limit
ulimit -n 700000

cluster_master_start()
{
  # Start the master process
  ./$BIN_DIR/apps/testbed/testbed \
    --cluster-role master \
    --cluster-ip ${CLUSTER_MASTER_IP} \
    --cluster-node-id ${CLUSTER_MASTER_NODE_ID} 2> $OUTPUT_DIR/master.log &
  CLUSTER_MASTER_PID=$!
}

cluster_slave_start()
{
  # Start the slave process
  ./$BIN_DIR/apps/testbed/testbed \
    --cluster-role slave \
    --cluster-ip ${CLUSTER_SLAVE_IP} \
    --cluster-master-ip ${CLUSTER_MASTER_IP} \
    --cluster-master-id ${CLUSTER_MASTER_NODE_ID} \
    --sim-ip ${CLUSTER_SLAVE_SIM_IP} \
    --sim-port-start ${CLUSTER_SLAVE_SIM_PORT_START} \
    --sim-port-end ${CLUSTER_SLAVE_SIM_PORT_END} \
    --sim-threads ${CLUSTER_SLAVE_SIM_THREADS} 2> $OUTPUT_DIR/slave.log &
  CLUSTER_SLAVE_PID=$!

  # Wait for the slave to register itself
  sleep 5
}

trap_handler()
{
  local pgid=$(ps -p $$ -o pgid="")
  kill -- -${pgid}
  exit
}
# Install trap to interrupt everything on failure
trap trap_handler INT QUIT TERM EXIT

echo ">>> Running multiple scenarios..."

for params in "${TOPOLOGIES[@]}"; do
  IFS=","
    set -- $params
    topology="$1"
    args="$2"
  IFS=" "
  mkdir -p $OUTPUT_DIR/$topology

  echo ">>> Topology: $topology"
  echo "    Arguments: $args"

  echo "  > Setting up testbed cluster..."
  echo "    - master"
  cluster_master_start
  echo "    - slave"
  cluster_slave_start

  echo "  > Running scenario '${SCENARIO}' via controller..."
  set -- \
    --cluster-role controller \
    --cluster-ip ${CLUSTER_CONTROLLER_IP} \
    --cluster-master-ip ${CLUSTER_MASTER_IP} \
    --cluster-master-id ${CLUSTER_MASTER_NODE_ID} \
    --topology $DATA_DIR/symmetric-topo-${topology}.graphml \
    --scenario ${SCENARIO} \
    --out-dir $OUTPUT_DIR/$topology/ \
    --id-gen consistent \
    --seed 1 \
    $args

  ./$BIN_DIR/apps/testbed/testbed "$@" 2> $OUTPUT_DIR/scenario-$topology.log || {
    tail -n 50 $OUTPUT_DIR/scenario-$topology.log
    echo "ERROR: Failed to run scenario!"
    exit 1
  }

  sleep 10
  echo "  > Cleaning up..."
  kill -9 ${CLUSTER_MASTER_PID} ${CLUSTER_SLAVE_PID}
  sleep 10
done
