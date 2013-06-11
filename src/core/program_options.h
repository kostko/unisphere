/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <k@jst.sm>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef UNISPHERE_CORE_PROGRAMOPTIONS_H
#define UNISPHERE_CORE_PROGRAMOPTIONS_H

#include "core/globals.h"

#include <boost/program_options.hpp>

namespace UniSphere {

/**
 * A simple class for building hierarchical program options modules.
 */
class UNISPHERE_EXPORT OptionModule {
public:
  /**
   * Performs initialization for top-level module.
   */
  virtual void initialize(int argc, char **argv);

  /**
   * Performs option setup and parsing of the program options.
   */
  virtual void initialize(int argc,
                          char **argv,
                          boost::program_options::options_description &options);
protected:
  /**
   * This method should handle program options. It will be called twice, first
   * with an empty variables map to setup the available options. After the options
   * are parsed, it will be called again with the variables in order to validate
   * them and to perhaps call any submodules. Calling submodules should simply
   * be done by calling initialize(argc, argv, options) on them.
   */
  virtual void setupOptions(int argc,
                            char **argv,
                            boost::program_options::options_description &options,
                            boost::program_options::variables_map &variables) {};
};

}

#endif
