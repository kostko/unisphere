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