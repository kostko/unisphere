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
    parser.add_argument('--input', metavar = ('FILE', 'COLUMN', 'LEGEND'), type = str, nargs = 3,
                        action = 'append', required = True,
                        help = 'input FILE and COLUMN whose CDF to take')

    parser.add_argument('--output', metavar = 'FILE', type = str,
                        help = 'output filename')

    parser.add_argument('--range', metavar = ('MIN', 'MAX'), type = int, nargs = 2,
                        help = 'X axis range')

    parser.add_argument('--xlabel', metavar = 'LABEL', type = str,
                        help = 'X axis label')

  def run(self, args):
    for filename, column, legend in args.input:
      try:
        data = pandas.read_csv(filename)
      except:
        args.parser.error('specified input FILE "%s" cannot be parsed' % filename)

      try:
        sample = numpy.asarray(data[column])
      except KeyError:
        args.parser.error('specified COLUMN "%s" does not exist in input file' % column)

      ecdf = sm.distributions.ECDF(sample)
      x = numpy.linspace(min(sample), max(sample))
      y = ecdf(x)

      plt.step(x, y, label = legend)

    plt.legend(loc = 'lower right')
    ax = plt.gca()
    ax.set_ylabel('CDF')
    if args.xlabel:
      ax.set_xlabel(args.xlabel)

    if args.range:
      plt.axis([args.range[0], args.range[1], 0.0, 1.01])
    else:
      plt.axis([1.0, max(sample), 0.0, 1.01])

    plt.grid()

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
