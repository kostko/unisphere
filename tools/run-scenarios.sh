#!/bin/bash

BIN_DIR="build"
DATA_DIR="data"
OUTPUT_DIR="output"
TOPOLOGIES=(
  "n16,--max-runtime 120"
  "n32,--max-runtime 120"
  "n64,--max-runtime 120"
  "n128,--max-runtime 120"
  "n256,--max-runtime 240"
)

if [[ ! -d $BIN_DIR || ! -d $DATA_DIR || ! -d $OUTPUT_DIR ]]; then
  echo "ERROR: Not being run from proper location!"
  exit 1
fi

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
    --scenario SimpleTestScenario \
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

