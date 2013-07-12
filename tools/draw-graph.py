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

def float_with_none(value):
  if value == 'N':
    return None
  return float(value)

class ModuleCDF(object):
  command = 'cdf'
  description = 'generates empiric CDF plots'

  def arguments(self, parser):
    parser.add_argument('--input', metavar = ('FILE', 'COLUMN', 'LEGEND'), type = str, nargs = 3,
                        action = 'append', required = True,
                        help = 'input FILE and COLUMN whose CDF to take')

    parser.add_argument('--output', metavar = 'FILE', type = str,
                        help = 'output filename')

    parser.add_argument('--xrange', metavar = ('MIN', 'MAX'), type = float_with_none, nargs = 2,
                        help = 'X axis range')

    parser.add_argument('--yrange', metavar = ('MIN', 'MAX'), type = float_with_none, nargs = 2,
                        default = (0.0, 1.01),
                        help = 'Y axis range')

    parser.add_argument('--xlabel', metavar = 'LABEL', type = str,
                        help = 'X axis label')

  def run(self, args):
    styles = [
      ('-', None),
      ('--', None),
      ('-.', None),
      ('-', 'o'),
      ('--', '^'),
      ('--', 's')
    ]

    for idx, (filename, column, legend) in enumerate(args.input):
      try:
        data = pandas.read_csv(filename, sep = None)
      except:
        args.parser.error('specified input FILE "%s" cannot be parsed' % filename)

      try:
        sample = numpy.asarray(data[column].dropna())
      except KeyError:
        args.parser.error('specified COLUMN "%s" does not exist in input file' % column)

      ecdf = sm.distributions.ECDF(sample)
      x = numpy.linspace(min(sample), max(sample))
      y = ecdf(x)

      ls, marker = styles[idx % len(styles)]
      plt.plot(x, y, drawstyle = 'steps', linestyle = ls, linewidth = 2, marker = marker, markevery = 2, label = legend)

    plt.legend(loc = 'lower right')
    plt.grid()
    ax = plt.gca()
    ax.set_axisbelow(True)
    ax.set_ylabel('Cummulative Probability')
    if args.xlabel:
      ax.set_xlabel(args.xlabel)

    if args.xrange:
      plt.axis([args.xrange[0], args.xrange[1], args.yrange[0], args.yrange[1]])
    else:
      plt.axis([0.0, max(sample), args.yrange[0], args.yrange[1]])

    if args.output:
      plt.savefig(args.output)
    else:
      plt.show()

class ModuleSimplePlot(object):
  command = 'plot'
  description = 'generate simple plots'

  def arguments(self, parser):
    parser.add_argument('--input', metavar = ('FILE', 'COLUMN', 'VALUE'), type = str, nargs = 3,
                        action = 'append', required = True,
                        help = 'input FILE and COLUMN to get the Y-axis data from')

    parser.add_argument('--output', metavar = 'FILE', type = str,
                        help = 'output filename')

    parser.add_argument('--ylabel', metavar = 'LABEL', type = str,
                        help = 'Y axis label')

    parser.add_argument('--xlabel', metavar = 'LABEL', type = str,
                        help = 'X axis label')

    parser.add_argument('--fit', metavar = 'FUNC', type = str,
                        help = 'function to fit and include (must define a lambda function)')

    parser.add_argument('--fit-range', metavar = 'X', type = float, default = 5.0)

    parser.add_argument('--fit-label', metavar = 'LABEL', type = str,
                        help = 'label to use for fitted function in the legend')

  def run(self, args):
    X = []
    Y = []
    Yerr = []
    for filename, column, value in args.input:
      try:
        data = pandas.read_csv(filename)
      except:
        args.parser.error('specified input FILE "%s" cannot be parsed' % filename)

      try:
        sample = numpy.asarray(data[column])
      except KeyError:
        args.parser.error('specified COLUMN "%s" does not exist in input file' % column)

      X += [float(value)]
      Y += [numpy.average(sample)]
      Yerr += [numpy.std(sample)]

    plt.errorbar(X, Y, Yerr, label = 'Measurements')

    if args.fit:
      import scipy.optimize
      func = eval(args.fit)

      popt, pcov = scipy.optimize.curve_fit(func, X, Y)
      Fx = numpy.linspace(min(X), max(X) + args.fit_range*(X[-1] - X[-2]), 100)
      Fy = [func(x, *popt) for x in Fx]
      plt.plot(Fx, Fy, linestyle = '--', color = 'black', label = args.fit_label)
    
    ax = plt.gca()
    if args.ylabel:
      ax.set_ylabel(args.ylabel)
    if args.xlabel:
      ax.set_xlabel(args.xlabel)

    plt.grid()
    plt.legend(loc = 'lower right')

    if args.output:
      plt.savefig(args.output)
    else:
      plt.show()

# A list of registered modules
modules = [
  ModuleCDF,
  ModuleSimplePlot
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
