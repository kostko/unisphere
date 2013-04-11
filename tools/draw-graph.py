#!/usr/bin/python
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
import argparse
import matplotlib.pyplot as plt
import numpy
import pandas
import statsmodels.api as sm

def comma_separated_strings(value):
  return value.split(",")

class ModuleCDF(object):
  command = 'cdf'
  description = 'generates empiric CDF plots'

  def arguments(self, parser):
    parser.add_argument('filename', metavar = 'FILE', type = str,
                        help = 'input filename in CSV format')

    parser.add_argument('column', metavar = 'COLUMN', type = str,
                        help = 'column whose CDF to take')

    parser.add_argument('--output', metavar = 'FILE', type = str,
                        help = 'output filename')

  def run(self, args):
    try:
      data = pandas.read_csv(args.filename)
    except:
      args.parser.error('specified input FILE cannot be parsed')

    try:
      sample = numpy.asarray(data[args.column])
    except KeyError:
      args.parser.error('specified COLUMN does not exist in input file')

    ecdf = sm.distributions.ECDF(sample)
    x = numpy.linspace(min(sample), max(sample))
    y = ecdf(x)

    plt.step(x, y)
    plt.axis([1.0, max(sample), 0.0, 1.01])

    if args.output:
      plt.savefig(args.output)
    else:
      plt.show()

# A list of registered modules
modules = [
  ModuleCDF
]

main_parser = argparse.ArgumentParser("draw-graph")
subparsers = main_parser.add_subparsers(title = 'subcommands',
                                        description = 'valid subcommands',
                                        help = 'additional help')

for module_cls in modules:
  module = module_cls()
  module_parser = subparsers.add_parser(module.command, help = module.description)
  module.arguments(module_parser)
  module_parser.set_defaults(module = module, parser = module_parser)

args = main_parser.parse_args()
args.module.run(args)
