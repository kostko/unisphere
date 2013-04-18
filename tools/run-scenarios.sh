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
ulimit -n 1000000

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

  set -- \
    --scenario SingleStretchScenario \
    --phy-ip 127.0.0.1 \
    --phy-port 8472 \
    --out-dir $OUTPUT_DIR/$topology/ \
    --topology $DATA_DIR/symmetric-topo-${topology}.graphml \
    --id-gen consistent \
    --seed 1 $args

  ./$BIN_DIR/apps/testbed/testbed "$@" > $OUTPUT_DIR/scenario-$topology.log || {
    tail -n 50 $OUTPUT_DIR/scenario-$topology.log
    echo "ERROR: Failed to run scenario!"
    exit 1
  }
done

