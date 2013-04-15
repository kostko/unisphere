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

echo ">>> Drawing graphs..."

function draw_aggregate_cdf()
{
  variable="$1"
  xlabel="$2"
  input_file="$3"
  range_min="$4"
  range_max="$5"

  inputs=()
  variables=()
  labels=()
  for params in "${TOPOLOGIES[@]}"; do
    IFS=","
      set -- $params
      topology="$1"
      args="$2"
    IFS=" "

    output="$OUTPUT_DIR/$topology"

    # Get a list of all stretch outputs and pick the last one
    data=($output/$input_file)
    if [[ -z $data || "$data" == "$output/$input_file" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi

    inputs+=(${data[${#data[@]} - 1]})
    variables+=("$variable")
    labels+=("n = ${topology:1}")
  done

  args=()
  for i in "${!inputs[@]}"; do
    input=${inputs[$i]}
    variable=${variables[$i]}
    label=${labels[$i]}

    args+=('--input' "$input" "$variable" "$label")
  done

  args+=('--output' "$OUTPUT_DIR/cdf_$variable.pdf" '--xlabel' "$xlabel" '--range' "$range_min" "$range_max")

  ./tools/draw-graph.py cdf "${args[@]}"
}

function draw_vs_size_plot()
{
  variable="$1"
  ylabel="$2"
  input_file="$3"
  fit_function="$4"
  fit_range="$5"

  inputs=()
  variables=()
  sizes=()
  for params in "${TOPOLOGIES[@]}"; do
    IFS=","
      set -- $params
      topology="$1"
      args="$2"
    IFS=" "

    output="$OUTPUT_DIR/$topology"

    # Get a list of all stretch outputs and pick the last one
    data=($output/$input_file)
    if [[ -z $data || "$data" == "$output/$input_file" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi

    inputs+=(${data[${#data[@]} - 1]})
    variables+=("$variable")
    sizes+=("${topology:1}")
  done

  args=()
  for i in "${!inputs[@]}"; do
    input=${inputs[$i]}
    variable=${variables[$i]}
    size=${sizes[$i]}

    args+=('--input' "$input" "$variable" "$size")
  done

  args+=('--output' "$OUTPUT_DIR/plot_${variable}_vs_size.pdf" '--xlabel' "Topology size" '--ylabel' "$ylabel" '--fit' "$fit_function" '--fit-range' "$fit_range")

  ./tools/draw-graph.py plot "${args[@]}"
}

# Call all draw functions
draw_aggregate_cdf stretch "Path stretch" "routing-all_pairs-stretch-*.csv" 0.9 N
draw_aggregate_cdf rt_active "Routing state" "state-count-state-*.csv" 0 N
draw_vs_size_plot rt_active "Routing state" "state-count-state-*.csv" "lambda x, a, b: a*numpy.sqrt(x) + b" 2
