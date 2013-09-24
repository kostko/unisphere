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
BIN_DIR="build/release"
DATA_DIR="data"
OUTPUT_DIR="output"
TOPOLOGIES=(
  "n16,"
  "n32,"
  "n64,"
  "n128,"
  "n256,"
  "n512,"
  "n1024,"
)
SCENARIO="StandardTests"

CLUSTER_MASTER_IP="127.0.0.1"
CLUSTER_MASTER_NODE_ID="02f1076098d2c57f70ef4bfe35cc8bdfad806f60"
CLUSTER_CONTROLLER_IP="127.0.0.2"
CLUSTER_SLAVE_IP="127.0.0.3"
CLUSTER_SLAVE_SIM_IP="127.0.1.1"
CLUSTER_SLAVE_SIM_PORT_START=9000
CLUSTER_SLAVE_SIM_PORT_END=20000
CLUSTER_SLAVE_SIM_THREADS=$(cat /proc/cpuinfo | grep processor | wc -l)

if [[ ! -d $BIN_DIR || ! -d $DATA_DIR || ! -d $OUTPUT_DIR ]]; then
  echo "ERROR: Not being run from proper location!"
  exit 1
fi

# Setup a different scenario when specified
if [[ ! -z "$1" ]]; then
  SCENARIO="$1"
fi

# Limit topologies to just a single one when specified
if [[ ! -z "$2" ]]; then
  filter="$2"
  tmp=()
  for params in "${TOPOLOGIES[@]}"; do
    IFS=","
      set -- $params
      topology="$1"
      args="$2"
    IFS=" "

    if [[ "$topology" == "$filter" ]]; then
      tmp+=("$topology,$args")
    fi
  done

  TOPOLOGIES=("${tmp[@]}")
fi
