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

function draw_stretch_cdf()
{
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
    data=($output/routing-all_pairs-stretch-*.csv)
    if [[ -z $data || "$data" == "$output/routing-all_pairs-stretch-*.csv" ]]; then
      echo "WARNING: Missing data file for topology '$topology'."
      continue
    fi

    inputs+=(${data[${#data[@]} - 1]})
    variables+=('stretch')
    labels+=("n = ${topology:1}")
  done

  args=()
  for i in "${!inputs[@]}"; do
    input=${inputs[$i]}
    variable=${variables[$i]}
    label=${labels[$i]}

    args+=('--input' "$input" "$variable" "$label")
  done

  args+=('--output' cdf_stretch.pdf '--xlabel' "Path stretch")

  ./tools/draw-graph.py cdf "${args[@]}"
}

# Call all draw functions
draw_stretch_cdf
