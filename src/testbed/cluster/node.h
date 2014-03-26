/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_TESTBED_CLUSTERNODE_H
#define UNISPHERE_TESTBED_CLUSTERNODE_H

#include "core/globals.h"
#include "core/program_options.h"

#include <boost/program_options.hpp>

namespace UniSphere {

class NodeIdentifier;
class Context;
class LinkManager;
class InterplexRpcChannel;

template <typename Channel>
class RpcEngine;

namespace TestBed {

/**
 * A base class for implementing different roles in a testbed
 * cluster.
 */
class UNISPHERE_EXPORT ClusterNode : public OptionModule {
public:
  /**
   * Class constructor.
   */
  ClusterNode();

  ClusterNode(const ClusterNode&) = delete;
  ClusterNode &operator=(const ClusterNode&) = delete;

  /**
   * Starts this cluster node.
   *
   * @return Program exit code
   */
  int start();
protected:
  /**
   * Sets up command line options and initializes the cluster node.
   *
   * @param argc Number of command line arguments
   * @param argv Command line arguments
   * @param options Program options parser configuration
   * @param variables Prgoram option variables
   */
  void setupOptions(int argc,
                    char **argv,
                    boost::program_options::options_description &options,
                    boost::program_options::variables_map &variables);

  /**
   * Actual cluster node implementations should override this method
   * to perform actual work here.
   */
  virtual void run() {};
protected:
  /**
   * Returns the testbed's UNISPHERE context.
   */
  Context &context();

  /**
   * Returns the UNISPHERE link manager used for communication between
   * the testbed cluster nodes.
   */
  LinkManager &linkManager();

  /**
   * Returns the RPC engine used for calling methods between the testbed
   * cluster nodes.
   */
  RpcEngine<InterplexRpcChannel> &rpc();

  /**
   * Stops the cluster node and exits with a zero exit code.
   */
  void stop();

  /**
   * Stops the  cluster node and exits with a non-zero exit code.
   */
  void fail();
private:
  UNISPHERE_DECLARE_PRIVATE(ClusterNode)
};

UNISPHERE_SHARED_POINTER(ClusterNode)

}

}

#endif
