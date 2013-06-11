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
#ifndef UNISPHERE_TESTBED_MASTER_H
#define UNISPHERE_TESTBED_MASTER_H

#include "testbed/cluster/node.h"

namespace UniSphere {

namespace TestBed {

class UNISPHERE_EXPORT Master : public ClusterNode {
public:
  Master();

  Master(const Master&) = delete;
  Master &operator=(const Master&) = delete;
protected:
  void setupOptions(int argc,
                    char **argv,
                    boost::program_options::options_description &options,
                    boost::program_options::variables_map &variables);

  void run();
private:
  UNISPHERE_DECLARE_PRIVATE(Master)
};

}

}

#endif
