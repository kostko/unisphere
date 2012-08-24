/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#include "core/context.h"
#include "interplex/link_manager.h"
#include "plexus/bootstrap.h"
#include "plexus/router.h"

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/format.hpp>

namespace po = boost::program_options;
using boost::format;
using namespace UniSphere;

int main(int argc, char **argv)
{
  LibraryInitializer init;
  
  // Parse program options
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "show help message")
    ("id", po::value<std::string>(), "local node id in hex format")
    ("host", po::value<std::string>(), "listen ip")
    ("port", po::value<unsigned short>(), "listen port")
    ("peer-id", po::value<std::string>(), "bootstrap peer node id in hex format")
    ("peer-host", po::value<std::string>(), "bootstrap peer ip")
    ("peer-port", po::value<unsigned short>(), "bootstrap peer port")
  ;
  
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (std::exception &e) {
    std::cout << "ERROR: There is an error in your invocation arguments!" << std::endl;
    std::cout << desc << std::endl;
    return 1;
  }
  
  if (vm.count("help")) {
    // Handle help option
    std::cout << "UNISPHERE Plexus Test Application" << std::endl;
    std::cout << std::endl;
    std::cout << desc << std::endl;
    return 1;
  } else {
    if (!vm.count("id") || !vm.count("host") || !vm.count("port") || !vm.count("peer-id") || !vm.count("peer-host") || !vm.count("peer-port")) {
      std::cout << "ERROR: Missing arguments!" << std::endl;
      std::cout << desc << std::endl;
      return 2;
    }
  }
  
  // Create the UNISPHERE context, node identifier and link manager
  Context ctx;
  NodeIdentifier nodeId(vm["id"].as<std::string>(), NodeIdentifier::Format::Hex);
  LinkManager mgr(ctx, nodeId);
  
  // Listen on specified address
  mgr.listen(Address(vm["host"].as<std::string>(), vm["port"].as<unsigned short>()));

  // Setup the bootstrap method
  NodeIdentifier peerId(vm["peer-id"].as<std::string>(), NodeIdentifier::Format::Hex);
  Contact peerContact(peerId);
  peerContact.addAddress(Address(vm["peer-host"].as<std::string>(), vm["peer-port"].as<unsigned short>()));
  SingleHostBootstrap bootstrap(peerContact);
  
  // Create the overlay router
  Router router(mgr, bootstrap);
  router.join();
  
  // Run the context
  ctx.run();
  return 0;
}
