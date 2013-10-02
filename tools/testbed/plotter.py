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

from .catalog import catalog
from . import exceptions

import argparse
import fnmatch
import hashlib
import importlib
import logging
import logging.config
import os
import sys
import traceback

logger = logging.getLogger('testbed.plotter')

class Plotter(object):
  def run(self, settings):
    """
    Scenario runner entry point.
    """
    main_parser = argparse.ArgumentParser("plotter")
    main_parser.add_argument('run_groups', metavar='run_id', type=str, nargs='+',
                             help='unique run identifier')
    main_parser.add_argument('--graphs', metavar='name', type=str, nargs='+',
                             help='limit to specific graphs')
    args = main_parser.parse_args()

    # Setup logging to stderr
    logging.config.dictConfig(settings.LOGGING)
    logger.info("Testbed root: %s" % settings.TESTBED_ROOT)

    logger.info("Loading run catalog...")
    catalog.load(settings)

    logger.info("Processing %d run groups..." % len(args.run_groups))
    for run_id in args.run_groups:
      logger.info("Processing run group '%s'..." % run_id)

      # Check if the specified run group output exists
      out_dir = os.path.join(settings.OUTPUT_DIRECTORY, run_id)
      if not os.path.exists(out_dir):
        logger.warning("Skipping run group '%s' as no output has been found!" % run_id)
        continue

      # Find out which runs exist for this run group and process all graphs
      # with dependencies among these runs
      runs = set()
      for run_descriptor in catalog.runs():
        if not os.path.isdir(os.path.join(out_dir, run_descriptor.name)):
          logger.warning("Run '%s' is missing from run group '%s'." % (run_descriptor.name, run_id))
          continue

        runs.add(run_descriptor.name)

      for graph in catalog.graphs():
        if args.graphs:
          for pattern in args.graphs:
            if fnmatch.fnmatch(graph.name, pattern):
              break
          else:
            logger.info("Skipping graph '%s'." % graph.name)
            continue

        if runs.intersection(graph.runs) != set(graph.runs):
          logger.warning("Skipping graph '%s' because of unsatisfied run dependencies." % graph.name)
          continue

        logger.info("Plotting graph '%s' from %d runs..." % (graph.name, len(graph.runs)))
        for key, value in graph.settings.items():
          logger.info("  [%s] = %s" % (key, value))
        try:
          graph.plot(run_id, catalog.get_run_descriptors(graph.runs), settings)
        except KeyboardInterrupt:
          logger.info("Abort requested by user.")
          return
        except NotImplementedError:
          logger.warning("Skipping non-implemented graph plotter '%s.%s'." % 
            (graph.plotter.__module__, graph.plotter.__name__))
          continue
        except exceptions.MissingDatasetError:
          logger.warning("Skipping graph '%s' due to missing dataset." % graph.name)
          continue
        except:
          logger.error("Aborting due to error.")
          logger.error(traceback.format_exc())
          return

        logger.info("Graph '%s' done." % graph.name)

      logger.info("Run group '%s' done." % run_id)
