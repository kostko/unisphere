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

class ModuleProfiling(object):
  command = 'prof'
  description = 'generates profiling plots'

  def arguments(self, parser):
    parser.add_argument('--input', metavar = 'FILE', type = str, required = True,
                        help = 'input profiling data FILE')

    parser.add_argument('--output', metavar = 'FILE', type = str,
                        help = 'output filename')

    parser.add_argument('--by-tag', action = 'store_true', default = False,
                        help = 'group measurements by tag')

    parser.add_argument('--by-name', action = 'store_true', default = False,
                        help = 'group measurements by name')

  def run(self, args):
    styles = [
      ('-', None),
      ('--', None),
      ('-.', None),
      ('-', 'o'),
      ('--', '^'),
      ('--', 's')
    ]

    try:
      data = pandas.read_csv(args.input, sep = None,
        names = ['timestamp', 'duration', 'node_id', 'channel', 'name', 'tags'],
        converters = dict(tags = lambda x: tuple([y for y in x.split(':') if len(y)])))
    except:
      args.parser.error('specified input FILE "%s" cannot be parsed' % args.input)

    if args.by_tag:
      column = 'tags'
      tags = set([tag for tags in data['tags'].dropna() for tag in tags])
      mapper = lambda tag: lambda x: tag in x
    elif args.by_name:
      column = 'name'
      tags = set(data['name'].dropna())
      mapper = lambda tag: lambda x: x == tag

    durations = numpy.asarray(data['duration'].dropna())
    max_duration = 0
    
    for idx, tag in enumerate(tags):
      sample = data[data[column].map(mapper(tag))]
      sample = numpy.asarray(sample['duration'].dropna())
      max_duration = max(max(sample), max_duration)

      ecdf = sm.distributions.ECDF(sample)
      x = numpy.linspace(min(sample), max(sample))
      y = ecdf(x)

      ls, marker = styles[idx % len(styles)]
      plt.plot(x, y, drawstyle = 'steps', linestyle = ls, linewidth = 2, marker = marker, markevery = 2, label = tag)

    plt.legend(loc = 'lower right')
    plt.grid()
    ax = plt.gca()
    ax.set_axisbelow(True)
    ax.set_ylabel('Cummulative Probability')
    ax.set_xlabel('ns')
    plt.axis([0.0, max_duration, 0.0, 1.01])

    if args.output:
      plt.savefig(args.output)
    else:
      plt.show()

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
        data = pandas.read_csv(filename, sep=None)
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

class ModuleTimeSeriesPlot(object):
  command = 'timeseries'
  description = 'generate time series plots'

  def arguments(self, parser):
    parser.add_argument('--input', metavar=('FILE', 'XCOLUMN', 'YCOLUMN', 'GCOLUMN', 'LABEL', 'COLOR'), type=str, nargs=6,
                        action='append', required = True,
                        help='input FILE and columns to generate the plot from')

    parser.add_argument('--rate', action='store_true',
                        help='plot rate instead of value')

    parser.add_argument('--moving-avg', action='store_true',
                        help='draw moving averages')

    parser.add_argument('--autoscale-xrange', action='store_true',
                        help='automatically adjust X axis')

    parser.add_argument('--output', metavar='FILE', type=str,
                        help='output filename')

    parser.add_argument('--ylabel', metavar='LABEL', type=str,
                        help='Y axis label')

    parser.add_argument('--xlabel', metavar='LABEL', type=str,
                        help='X axis label')

  def run(self, args):
    min_max_ts = None
    for filename, xcolumn, ycolumn, gcolumn, label, color in args.input:
      try:
        data = pandas.read_csv(filename, sep=None)
      except:
        args.parser.error('specified input FILE "%s" cannot be parsed' % filename)

      ts_base = min(data[xcolumn])
      groups = set(data[gcolumn].unique())
      data.sort(xcolumn, inplace=True)
      
      group_status = {}
      prev_ts = None
      current_ts = ts_base

      for _, element in data.iterrows():
        group_status.setdefault(element[xcolumn], {})[element[gcolumn]] = element[ycolumn]
        if current_ts != element[xcolumn]:
          # ts has changed
          if len(group_status[current_ts]) != len(groups):
            for group in groups.difference(group_status[current_ts].keys()):
              group_status[current_ts][group] = group_status[prev_ts][group]

          prev_ts = current_ts
          current_ts = element[xcolumn]

      X = numpy.asarray(sorted(group_status.keys()))
      Y = [numpy.average(group_status[x].values()) for x in X]
      X -= ts_base

      # discard last 10 measurements
      X = X[:-10]
      Y = Y[:-10]

      if min_max_ts is None or X[-1] < min_max_ts:
        min_max_ts = X[-1]

      if args.rate:
        def make_rate(xr, yr):
          Yrate = []
          for x1, x2, y1, y2 in zip(xr, xr[1:], yr, yr[1:]):
            Yrate += [(y2 - y1) / (x2 - x1)]
          
          return xr[1:], Yrate

        X, Y = make_rate(X, Y)

      if args.moving_avg:
        plt.plot(X, Y, color=color, alpha=0.4, zorder=0)

        w = 60
        Ymean = numpy.asarray(pandas.rolling_mean(pandas.Series([0]*w + Y), w))
        plt.plot(X, Ymean[w:], label=label, color=color, zorder=1)
      else:
        plt.plot(X, Y, color=color, alpha=0.9, zorder=0, label=label)

    ax = plt.gca()
    if args.ylabel:
      ax.set_ylabel(args.ylabel)
    if args.xlabel:
      ax.set_xlabel(args.xlabel)

    if args.autoscale_xrange:
      plt.xlim(0, min_max_ts)

    plt.grid()
    leg = plt.legend(loc='upper right', fontsize='small', fancybox=True)
    leg.get_frame().set_alpha(0.8)

    if args.output:
      plt.savefig(args.output)
    else:
      plt.show()

# A list of registered modules
modules = [
  ModuleCDF,
  ModuleSimplePlot,
  ModuleProfiling,
  ModuleTimeSeriesPlot
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
