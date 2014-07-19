#
# This file is part of UNISPHERE.
#
# Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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

logger = logging.getLogger('testbed.runner')

class Runner(object):
  def run(self, settings):
    """
    Scenario runner entry point.
    """
    main_parser = argparse.ArgumentParser("runner")
    main_parser.add_argument('--runs', metavar='name', type=str, nargs='+',
                             help='limit to specific runs')
    args = main_parser.parse_args()

    # Setup logging to stderr
    logging.config.dictConfig(settings.LOGGING)
    logger.info("Testbed root: %s" % settings.TESTBED_ROOT)

    # Select cluster configuration
    cluster_cfg = settings.CLUSTERS[settings.CLUSTER]

    logger.info("Loading cluster runner '%s'..." % settings.CLUSTER)
    i = settings.CLUSTER.rfind('.')
    module, attr = settings.CLUSTER[:i], settings.CLUSTER[i + 1:]
    try:
      module = importlib.import_module(module)
      cluster = getattr(module, attr)(cluster_cfg, settings)
    except (ImportError, AttributeError):
      raise exceptions.ImproperlyConfigured("Error importing cluster runner '%s'!" % settings.CLUSTER)

    logger.info("Loading run catalog...")
    catalog.load(settings, graphs=False)

    # Generate unique run identifier so that we can be sure that all runs have the same version
    run_id = hashlib.md5(os.urandom(32)).hexdigest()[:5]

    logger.info("Executing all runs (run_id=%s)..." % run_id)
    for descriptor in catalog.runs():
      if args.runs:
        for pattern in args.runs:
          if fnmatch.fnmatch(descriptor.name, pattern):
            break
        else:
          logger.info("Skipping run '%s'." % descriptor.name)
          continue

      logger.info("Starting run '%s'" % descriptor.name)
      for key, value in descriptor.settings.items():
        logger.info("  [%s] = %s" % (key, value))

      try:
        descriptor.run(cluster, run_id)
      except KeyboardInterrupt:
        logger.info("Abort requested by user.")
        return
      except exceptions.ScenarioRunFailed:
        logger.error("Aborting due to scenario run error.")
        return
      except:
        logger.error("Aborting due to error.")
        logger.error(traceback.format_exc())
        return

      logger.info("Run '%s' completed." % descriptor.name)
