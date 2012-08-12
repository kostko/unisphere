
#include "core/context.h"
#include "interplex/link_manager.h"
#include "interplex/link.h"

#include "apps/iplex/hello.pb.h"

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
    ("peer-id", po::value<std::string>(), "peer node id in hex format")
    ("peer-host", po::value<std::string>(), "peer ip")
    ("peer-port", po::value<unsigned short>(), "peer port")
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
    std::cout << "UNISPHERE Interplex Test Application" << std::endl;
    std::cout << std::endl;
    std::cout << desc << std::endl;
    return 1;
  } else {
    if (!vm.count("id") || !vm.count("host") || !vm.count("port")) {
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
  
  // Subscribe to message received events
  mgr.setListenLinkInit([](Link &link) {
    link.signalMessageReceived.connect([](const Message &msg) {
      ::Protocol::Test::Hello pmsg = message_cast< ::Protocol::Test::Hello>(msg);
      std::cout << "Received msg: " << pmsg.msg() << std::endl;
      std::cout << "Sender: " << msg.originator()->nodeId().as(NodeIdentifier::Format::Hex) << std::endl;
    });
  });
  
  // Check if we should also connect somewhere
  if (vm.count("peer-id") && vm.count("peer-host") && vm.count("peer-port")) {
    NodeIdentifier peerId(vm["peer-id"].as<std::string>(), NodeIdentifier::Format::Hex);
    Contact peerContact(peerId);
    peerContact.addAddress(Address(vm["peer-host"].as<std::string>(), vm["peer-port"].as<unsigned short>()));
    
    // Connect to the peer node
    LinkPtr link = mgr.connect(peerContact);
    
    // Transmit some test message (this is all done in a non-blocking manner)
    ::Protocol::Test::Hello msg;
    msg.set_msg("hello interplex world!");
    link->send(Message(Message::Type::UserMsg1, msg));
  }
  
  // Run the context
  ctx.run();
  return 0;
}
