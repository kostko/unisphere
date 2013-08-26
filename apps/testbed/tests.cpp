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
#include "testbed/test_bed.h"
#include "testbed/dataset/graphs.hpp"
#include "testbed/dataset/csv.hpp"
#include "social/compact_router.h"
#include "social/routing_table.h"
#include "social/name_database.h"
#include "social/sloppy_group.h"
#include "social/rpc_channel.h"
#include "interplex/link_manager.h"
#include "rpc/engine.hpp"

#ifdef UNISPHERE_PROFILE
#include "social/profiling/message_tracer.h"
#endif

#include "src/social/core_methods.pb.h"

#include <atomic>
#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>

using namespace UniSphere;
using namespace UniSphere::TestBed;

namespace Tests {

class CountState : public TestCase
{
public:
  /// State dataset
  DataSet<> ds_state{"ds_state"};

  using TestCase::TestCase;

  /**
   * Count the amount of state a node is using.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    ds_state.add({
      { "node_name",    node->name },
      // Routing table state
      { "rt_all",       node->router->routingTable().size() },
      { "rt_active",    node->router->routingTable().sizeActive() },
      { "rt_vicinity",  node->router->routingTable().sizeVicinity() },
      // Name database state
      { "ndb_all",      node->router->nameDb().size() },
      { "ndb_active",   node->router->nameDb().sizeActive() },
      { "ndb_cache",    node->router->nameDb().sizeCache() }
    });

    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    BOOST_LOG(logger()) << "Sending " << ds_state.size() << " records in ds_state.";
    api.send(ds_state);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_state);
    BOOST_LOG(logger()) << "Received " << ds_state.size() << " records in ds_state.";

    outputCsvDataset(
      ds_state,
      { "node_name", "rt_all", "rt_active", "rt_vicinity", "ndb_all", "ndb_active", "ndb_cache" },
      api.getOutputFilename("state", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(CountState, "state/count")

class DumpSloppyGroupTopology : public TestCase
{
public:
  /// Graph topology type
  typedef SloppyGroupManager::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;
  /// Topology dataset
  DataSet<Graph::graph_type> ds_topology{"ds_topology"};

  using TestCase::TestCase;

  /**
   * Dump sloppy group topology on each node.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    node->router->sloppyGroup().dumpTopology(graph);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    ds_topology.add({ "graph", graph.graph() });
    BOOST_LOG(logger()) << "Sending " << boost::num_vertices(graph.graph()) << " vertices in ds_topology.";
    api.send(ds_topology);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = SloppyGroupManager::TopologyDumpTags;

    api.receive(ds_topology);
    mergeGraphDataset<Graph, Tags::NodeName>(ds_topology, "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("group", boost::get(Tags::NodeGroup(), graph.graph()));
    properties.property("is_foreign", boost::get(Tags::LinkIsForeign(), graph.graph()));
    properties.property("is_reverse", boost::get(Tags::LinkIsReverse(), graph.graph()));
    
    outputGraphDataset(graph, properties, api.getOutputFilename("sg-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpSloppyGroupTopology, "state/sloppy_group_topology")

class DumpRoutingTopology : public TestCase
{
public:
  /// Graph topology type
  typedef CompactRoutingTable::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;
  /// Topology dataset
  DataSet<Graph::graph_type> ds_topology{"ds_topology"};

  using TestCase::TestCase;

  /**
   * Dump routing topology on each node.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    node->router->routingTable().dumpTopology(graph);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    ds_topology.add({ "graph", graph.graph() });
    BOOST_LOG(logger()) << "Sending " << boost::num_vertices(graph.graph()) << " vertices in ds_topology.";
    api.send(ds_topology);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = CompactRoutingTable::TopologyDumpTags;

    api.receive(ds_topology);
    mergeGraphDataset<Graph, Tags::NodeName>(ds_topology, "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("group", boost::get(Tags::NodeGroup(), graph.graph()));
    properties.property("is_landmark", boost::get(Tags::NodeIsLandmark(), graph.graph()));
    properties.property("state", boost::get(Tags::NodeStateSize(), graph.graph()));
    properties.property("vport", boost::get(Tags::LinkVportId(), graph.graph()));
    
    outputGraphDataset(graph, properties, api.getOutputFilename("rt-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpRoutingTopology, "state/routing_topology")

class PairWisePing : public TestCase
{
public:
  /// Dependent routing topology dump
  DumpRoutingTopologyPtr rt_topology;
  /// Raw measurements
  DataSet<> ds_raw{"ds_raw"};
  /// Stretch measurements
  DataSet<> ds_stretch{"ds_stretch"};
  /// Node sampling
  std::uniform_real_distribution<double> sampler{0.0, 1.0};
  /// Mutex
  std::recursive_mutex mutex;
  /// Nodes pending RPC call
  std::list<std::function<void()>> pending;

  using TestCase::TestCase;

  void preSelection(TestCaseApi &api)
  {
    // Call dependent test case to compute the routing topolgy for us
    rt_topology = boost::static_pointer_cast<DumpRoutingTopology>(
      api.callTestCase("state/routing_topology"));
  }

  SelectedPartition::Node selectNode(const Partition &partition,
                                     const Partition::Node &node,
                                     TestCaseApi &api)
  {
    boost::property_tree::ptree args;

    // Specify identifiers that should be paired with this node
    for (const Partition &p : api.getPartitions()) {
      for (const Partition::Node &n : p.nodes) {
        if (n.contact.nodeId() == node.contact.nodeId())
          continue;

        // Sample a subset of nodes
        // TODO: Make this subset configurable per test case instance
        if (sampler(api.rng()) < 0.9)
          continue;

        args.add("nodes.node", n.contact.nodeId().hex());
      }
    }

    return SelectedPartition::Node{ node.contact.nodeId(), args };
  }

  void callNext(TestCaseApi &api)
  {
    RecursiveUniqueLock lock(mutex);

    if (pending.empty())
      return finish(api);

    auto next = pending.front();
    pending.pop_front();
    api.defer(next);
  }

  /**
   * Perform all-pairs reachability testing.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    RecursiveUniqueLock lock(mutex);
    RpcEngine<SocialRpcChannel> &rpc = node->router->rpcEngine();

    // We should test one node at a time, to prevent overloading the network
    for (const auto &p : args.get_child("nodes", boost::property_tree::ptree())) {
      NodeIdentifier nodeId(p.second.data(), NodeIdentifier::Format::Hex);
      pending.push_back([this, &api, &rpc, node, nodeId]() {
        using namespace std::chrono;
        auto xmitTime = high_resolution_clock::now();
        Protocol::PingRequest request;
        request.set_timestamp(1);

        rpc.call<Protocol::PingRequest, Protocol::PingResponse>(nodeId, "Core.Ping", request,
          [this, &api, node, nodeId, xmitTime](const Protocol::PingResponse &rsp, const RoutedMessage &msg) {
            ds_raw.add({
              { "timestamp", boost::posix_time::microsec_clock::universal_time() },
              { "node_a", node->nodeId.hex() },
              { "node_b", nodeId.hex() },
              { "success", true },
#ifdef UNISPHERE_PROFILE
              { "msg_id", node->router->msgTracer().getMessageId(msg) },
#endif
              { "hops", (int) msg.hopDistance() },
              { "rtt", duration_cast<microseconds>(high_resolution_clock::now() - xmitTime).count() }
            });
            callNext(api);
          },
          [this, &api, node, nodeId](RpcErrorCode code, const std::string &msg) {
            ds_raw.add({
              { "timestamp", boost::posix_time::microsec_clock::universal_time() },
              { "node_a", node->nodeId.hex() },
              { "node_b", nodeId.hex() },
              { "success", false }
            });
            callNext(api);
          },
          rpc.options().setTimeout(15)
        );
      });
    }
  }

  void localNodesRunning(TestCaseApi &api)
  {
    // Start executing ping calls
    BOOST_LOG(logger()) << "Pinging " << pending.size() << " node pairs.";
    callNext(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    BOOST_LOG(logger()) << "Ping calls completed.";
    api.send(ds_raw);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_raw);

    // Output RAW dataset received from slaves
    outputCsvDataset(
      ds_raw,
      { "timestamp", "node_a", "node_b", "msg_id", "success", "hops", "rtt" },
      api.getOutputFilename("raw", "csv")
    );

    // Run all-pairs shortest paths algorithm on the obtained topology
    auto &topology = rt_topology->graph;
    typedef typename boost::graph_traits<CompactRoutingTable::TopologyDumpGraph>::vertex_descriptor vertex_des;
    std::unordered_map<vertex_des, std::unordered_map<vertex_des, int>> shortestLengths;
    boost::floyd_warshall_all_pairs_shortest_paths(topology.graph(), shortestLengths,
      boost::weight_map(boost::get(CompactRoutingTable::TopologyDumpTags::LinkWeight(), topology.graph())));

    // Compute path stretches for each raw measurement pair
    for (const auto &record : ds_raw) {
      if (!boost::get<bool>(record.at("success")))
        continue;

      std::string nodeA = boost::get<std::string>(record.at("node_a"));
      std::string nodeB = boost::get<std::string>(record.at("node_b"));
      int measuredLength = boost::get<int>(record.at("hops"));
      int shortestLength = shortestLengths[topology.vertex(nodeA)][topology.vertex(nodeB)];
      double stretch = (double) measuredLength / (double) shortestLength;

      ds_stretch.add({
        { "timestamp", record.at("timestamp") },
        { "node_a", nodeA },
        { "node_b", nodeB },
        { "measured", measuredLength },
        { "shortest", shortestLength },
        { "stretch", stretch }
      });
    }

    outputCsvDataset(
      ds_stretch,
      { "node_a", "node_b", "measured", "shortest", "stretch" },
      api.getOutputFilename("stretch", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(PairWisePing, "routing/pair_wise_ping")

class StartMessageTrace : public TestCase
{
public:
  using TestCase::TestCase;

  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
#ifdef UNISPHERE_PROFILE
    node->router->msgTracer().start();
#endif
    finish(api);
  }
};

UNISPHERE_REGISTER_TEST_CASE(StartMessageTrace, "traces/start")

class EndMessageTrace : public TestCase
{
public:
  using TestCase::TestCase;

  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
#ifdef UNISPHERE_PROFILE
    node->router->msgTracer().end();
#endif
    finish(api);
  }
};

UNISPHERE_REGISTER_TEST_CASE(EndMessageTrace, "traces/end")

class GetMessageTraces : public TestCase
{
public:
  /// Traces dataset
  DataSet<> ds_traces{"ds_traces"};

  using TestCase::TestCase;

  /**
   * Retrieve packet traces.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
#ifdef UNISPHERE_PROFILE
    for (const auto &p : node->router->msgTracer().getTraceRecords()) {
      ds_traces.add({
        { "node_id",        node->nodeId.hex() },
        { "pkt_id",         p.first },
        { "timestamp",      p.second.at("timestamp") },
        { "src",            p.second.at("src") },
        { "dst",            p.second.at("dst") },
        { "dst_lr",         p.second.at("dst_lr") },
        { "route_duration", p.second.at("route_duration") },
        { "local",          p.second.at("local") },
        { "processed",      p.second.at("processed") }
      });
    }
#endif

    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    api.send(ds_traces);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_traces);

    outputCsvDataset(
      ds_traces,
      { "node_id", "pkt_id", "timestamp", "src", "dst", "dst_lr", "route_duration", "local", "processed" },
      api.getOutputFilename("traces", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(GetMessageTraces, "traces/retrieve")

class NdbConsistentSanityCheck : public TestCase
{
public:
  /// Name database dataset
  DataSet<> ds_ndb{"ds_ndb"};
  /// Group membership dataset
  DataSet<> ds_groups{"ds_groups"};

  using TestCase::TestCase;

  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    ds_groups.add({
      { "node_id",    node->nodeId.hex() },
      { "group_len",  node->router->sloppyGroup().getGroupPrefixLength() }
    });

    for (NameRecordPtr record : node->router->nameDb().getNames(NameRecord::Type::SloppyGroup)) {
      ds_ndb.add({
        { "node_id",    node->nodeId.hex() },
        { "record_id",  record->nodeId.hex() },
        { "ts",         static_cast<int>(record->timestamp) },
        { "seqno",      static_cast<int>(record->seqno) }
      });
    }
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    api.send(ds_ndb);
    api.send(ds_groups);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_ndb);
    api.receive(ds_groups);

    outputCsvDataset(
      ds_ndb,
      { "node_id", "record_id", "ts", "seqno" },
      api.getOutputFilename("raw", "csv")
    );

    // Build a per-node map of name records
    std::unordered_map<std::string, std::unordered_set<std::string>> globalNdb;
    for (const auto &record : ds_ndb) {
      globalNdb[boost::get<std::string>(record.at("node_id"))].insert(
        boost::get<std::string>(record.at("record_id")));
    }
    ds_ndb.clear();

    // Check record consistency
    bool consistent = true;
    size_t checkedRecords = 0;
    for (const auto &record : ds_groups) {
      std::string nodeStringId = boost::get<std::string>(record.at("node_id"));
      NodeIdentifier nodeId(nodeStringId, NodeIdentifier::Format::Hex);
      size_t groupPrefixLen = boost::get<size_t>(record.at("group_len"));
      NodeIdentifier groupPrefix = nodeId.prefix(groupPrefixLen);

      for (const auto &sibling : ds_groups) {
        std::string siblingStringId = boost::get<std::string>(sibling.at("node_id"));
        NodeIdentifier siblingId(siblingStringId, NodeIdentifier::Format::Hex);
        if (nodeId == siblingId)
          continue;

        if (siblingId.prefix(groupPrefixLen) != groupPrefix)
          continue;

        // Ensure that this node has our record
        checkedRecords++;
        if (!globalNdb[siblingStringId].count(nodeStringId)) {
          BOOST_LOG_SEV(logger(), log::error) << "NDB inconsistent, node " << siblingStringId << " misses record for " << nodeStringId << ".";
          consistent = false;
        }
      }
    }

    if (!consistent)
      BOOST_LOG_SEV(logger(), log::error) << "NDB inconsistent after checking " << checkedRecords << " records.";
    else
      BOOST_LOG(logger()) << "NDB consistent after checking " << checkedRecords << " records.";
  }
};

UNISPHERE_REGISTER_TEST_CASE(NdbConsistentSanityCheck, "sanity/check_consistent_ndb")

class GetPerformanceStatistics : public TestCase
{
public:
  /// General statistics dataset
  DataSet<> ds_stats{"ds_stats"};
  /// Link congestion dataset
  DataSet<> ds_links{"ds_links"};

  using TestCase::TestCase;

  void extractStatistics(TestCaseApi &api,
                         VirtualNodePtr node,
                         bool links)
  {
    const auto &statsRouter = node->router->statistics();
    const auto &statsSg = node->router->sloppyGroup().statistics();
    const auto &statsRt = node->router->routingTable().statistics();
    const auto &statsNdb = node->router->nameDb().statistics();
    const auto &statsLink = node->router->linkManager().statistics();

    const auto &rt = node->router->routingTable();
    const auto &ndb = node->router->nameDb();

    ds_stats.add({
      // Timestamp and node identifier
      { "ts",           static_cast<int>(api.getTime()) },
      { "node_id",      node->nodeId.hex() },
      // Messaging complexity
      { "rt_msgs",      statsRouter.entryXmits },
      { "rt_updates",   statsRt.routeUpdates },
      { "rt_exp",       statsRt.routeExpirations },
      { "ndb_inserts",  statsNdb.recordInsertions },
      { "ndb_updates",  statsNdb.recordUpdates },
      { "ndb_exp",      statsNdb.recordExpirations },
      { "ndb_refresh",  statsNdb.localRefreshes },
      { "sg_msgs",      statsSg.recordXmits },
      { "lm_sent",      statsLink.global.msgXmits },
      { "lm_rcvd",      statsLink.global.msgRcvd },
      // Local state complexity
      //   Routing table
      { "rt_s_all",     rt.size() },
      { "rt_s_act",     rt.sizeActive() },
      { "rt_s_vic",     rt.sizeVicinity() },
      //   Name database
      { "ndb_s_all",    ndb.size() },
      { "ndb_s_act",    ndb.sizeActive() },
      { "ndb_s_cac",    ndb.sizeCache() }
    });

    // Link congestion
    if (links) {
      for (const auto &link : statsLink.links) {
        ds_links.add({
          { "ts",       static_cast<int>(api.getTime()) },
          { "node_id",  node->nodeId.hex() },
          { "link_id",  link.first.hex() },
          { "msgs",     link.second.msgRcvd }
        });
      }
    }
  }

  /**
   * Gather some statistics.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    extractStatistics(api, node, true);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    api.send(ds_stats);
    api.send(ds_links);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_stats);
    api.receive(ds_links);

    outputCsvDataset(
      ds_stats,
      {
        "ts", "node_id",
        "rt_msgs", "rt_updates", "rt_exp",
        "ndb_inserts", "ndb_updates", "ndb_exp", "ndb_refresh",
        "sg_msgs",
        "rt_s_all", "rt_s_act", "rt_s_vic",
        "ndb_s_all", "ndb_s_act", "ndb_s_cac"
      },
      api.getOutputFilename("raw", "csv")
    );

    outputCsvDataset(
      ds_links,
      { "ts", "node_id", "link_id", "msgs" },
      api.getOutputFilename("links", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(GetPerformanceStatistics, "stats/performance")

class CollectPerformanceStatistics : public GetPerformanceStatistics
{
public:
  using GetPerformanceStatistics::GetPerformanceStatistics;

  void collect(TestCaseApi &api,
               VirtualNodePtr node)
  {
    extractStatistics(api, node, false);
    api.defer(boost::bind(&CollectPerformanceStatistics::collect, this, boost::ref(api), node), 1);
  }

  /**
   * Gather some statistics.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    collect(api, node);
  }

  void signalReceived(TestCaseApi &api,
                      const std::string &signal)
  {
    // Finish the test case as soon as a signal is received
    finish(api);
  }
};

UNISPHERE_REGISTER_TEST_CASE(CollectPerformanceStatistics, "stats/collect_performance")


}
