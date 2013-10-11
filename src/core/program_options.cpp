/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <jernej@kos.mx>
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
#include "core/program_options.h"

namespace po = boost::program_options;

namespace UniSphere {

void OptionModule::initialize(int argc, char **argv)
{
  po::options_description options;
  initialize(argc, argv, options);
}

void OptionModule::initialize(int argc,
                              char **argv,
                              po::options_description &options)
{
  po::variables_map vm;

  setupOptions(argc, argv, options, vm);

  auto parsed = po::command_line_parser(argc, argv).options(options).allow_unregistered().run();
  po::store(parsed, vm);
  po::notify(vm);

  setupOptions(argc, argv, options, vm);
}

}
