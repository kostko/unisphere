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
import importlib
import logging
import logging.config
import sys

logger = logging.getLogger('testbed.runner')

class Runner(object):
  def run(self, settings):
    """
    Scenario runner entry point.
    """
    main_parser = argparse.ArgumentParser("runner")
    # TODO: Command line arguments for selecting runs
    args = main_parser.parse_args()

    # Setup logging to stderr
    logging.config.dictConfig(settings.LOGGING)
    logger.info("Testbed root: %s" % settings.TESTBED_ROOT)

    # TODO: Support multiple clusters, currently only the first one is used
    cluster_module, cluster_cfg = settings.CLUSTER.items()[0]

    logger.info("Loading cluster runner '%s'..." % cluster_module)
    i = cluster_module.rfind('.')
    module, attr = cluster_module[:i], cluster_module[i + 1:]
    try:
      module = importlib.import_module(module)
      cluster = getattr(module, attr)(cluster_cfg, settings)
    except (ImportError, AttributeError):
      raise exceptions.ConfigurationError("Error importing cluster runner '%s'!" % cluster_module)

    logger.info("Loading run catalog...")
    catalog.load(settings)

    logger.info("Executing all runs...")
    for descriptor in catalog:
      logger.info("Starting run '%s'" % descriptor.name)
      for key, value in descriptor.settings.items():
        logger.info("  [%s] = %s" % (key, value))

      try:
        descriptor.run(cluster)
      except KeyboardInterrupt:
        logger.info("Abort requested by user.")
        return
      except:
        logger.info("Aborting due to error.")
        return

      logger.info("Run '%s' completed." % descriptor.name)
