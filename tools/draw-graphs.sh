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

OUTPUT_FORMAT="pdf"

echo ">>> Drawing graphs..."

function draw_aggregate_cdf()
{
  variable="$1"
  xlabel="$2"
  input_file="$3"
  xrange_min="$4"
  xrange_max="$5"
  yrange_min="$6"
  yrange_max="$7"
  prefix="$8"

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

    # Get a list of all outputs and pick the last one
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

  args+=('--output' "$OUTPUT_DIR/cdf_${prefix}${variable}.${OUTPUT_FORMAT}" '--xlabel' "$xlabel" '--xrange' "$xrange_min" "$xrange_max")
  args+=('--yrange' "$yrange_min" "$yrange_max")

  ./tools/draw-graph.py cdf "${args[@]}"
}

function draw_vs_size_plot()
{
  variable="$1"
  ylabel="$2"
  input_file="$3"
  fit_function="$4"
  fit_range="$5"
  fit_label="$6"

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

    # Get a list of all outputs and pick the last one
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

  args+=('--output' "$OUTPUT_DIR/plot_${variable}_vs_size.${OUTPUT_FORMAT}" '--xlabel' "Number of nodes" '--ylabel' "$ylabel" '--fit' "$fit_function" '--fit-range' "$fit_range")
  args+=('--fit-label' "$fit_label")

  ./tools/draw-graph.py plot "${args[@]}"
}

function draw_messaging_performance()
{
  topologies=($1)
  sg_colors=($2)
  rt_colors=($3)
  input_file="stats-collect_performance-raw-*.csv"

  inputs=()
  variables=()
  labels=()
  colors=()
  i=0
  for topology in "${topologies[@]}"; do
    output="$OUTPUT_DIR/$topology"

    # Get a list of all outputs and pick the last one
    data=($output/$input_file)
    if [[ -z $data || "$data" == "$output/$input_file" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi

    # Sloppy group records/sec
    inputs+=(${data[${#data[@]} - 1]})
    variables+=("sg_msgs")
    labels+=("SG records sent (n=${topology:1})")
    colors+=("${sg_colors[$i]}")

    # Routing table entries/sec
    inputs+=(${data[${#data[@]} - 1]})
    variables+=("rt_msgs")
    labels+=("RT entries sent (n=${topology:1})")
    colors+=("${rt_colors[$i]}")

    let i++
  done

  args=()
  for i in "${!inputs[@]}"; do
    input=${inputs[$i]}
    variable=${variables[$i]}
    label=${labels[$i]}
    color=${colors[$i]}

    args+=('--input' "$input" 'ts' "$variable" 'node_id' "$label" "$color")
  done

  args+=('--output' "$OUTPUT_DIR/plot_messaging_performance.${OUTPUT_FORMAT}" '--xlabel' "time [s]" '--ylabel' "records/sec")
  args+=('--rate' '--moving-avg' '--autoscale-xrange')

  ./tools/draw-graph.py timeseries "${args[@]}"
}

function draw_link_congestion()
{
  input_file_main="$1"
  input_file_sp="$2"

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

    # Get a list of all outputs and pick the last one
    data_main=($output/$input_file_main)
    if [[ -z $data_main || "$data_main" == "$output/$input_file_main" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi
    data_sp=($output/$input_file_sp)
    if [[ -z $data_sp || "$data_sp" == "$output/$input_file_sp" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi

    inputs+=(${data_main[${#data_main[@]} - 1]})
    variables+=("msgs")
    labels+=("URP (n = ${topology:1})")

    inputs+=(${data_sp[${#data_sp[@]} - 1]})
    variables+=("msgs")
    labels+=("SP (n = ${topology:1})")
  done

  args=()
  for i in "${!inputs[@]}"; do
    input=${inputs[$i]}
    variable=${variables[$i]}
    label=${labels[$i]}

    args+=('--input' "$input" "$variable" "$label")
  done

  args+=('--output' "$OUTPUT_DIR/link_congestion.${OUTPUT_FORMAT}" '--xlabel' "Link congestion" '--xrange' "0" "N")
  args+=('--yrange' "0" "1.01")

  ./tools/draw-graph.py cdf "${args[@]}"
}


# Call all draw functions
draw_aggregate_cdf stretch "Path stretch" "routing-pair_wise_ping-stretch-*.csv" 0 N 0 1.01
draw_aggregate_cdf rt_s_act "Routing state" "stats-performance-raw-*.csv" 0 N 0 1.01
draw_vs_size_plot rt_s_act "Routing state" "stats-performance-raw-*.csv" "lambda x, a, b: a*numpy.sqrt(x) + b" 2 'Fit of $a \sqrt{x} + c$'
draw_vs_size_plot ndb_s_act "Sloppy group state" "stats-performance-raw-*.csv" "lambda x, a, b: a*numpy.sqrt(x) + b" 2 'Fit of $a \sqrt{x} + c$'
draw_messaging_performance \
  "n128 n256 n512 n1024" \
  "#03899c #133aac #00c322 #4867d6" \
  "#ff8500 #ffb600 #ff2c00 #a62500"
draw_link_congestion "stats-collect_link_congestion-raw-*.csv" "stats-collect_link_congestion-sp-*.csv"
