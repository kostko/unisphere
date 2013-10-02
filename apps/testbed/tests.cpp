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
#include "core/operators.h"
#include "testbed/test_bed.h"
#include "testbed/dataset/graphs.hpp"
#include "testbed/dataset/csv.hpp"
#include "social/compact_router.h"
#include "social/routing_table.h"
#include "social/name_database.h"
#include "social/sloppy_group.h"
#include "social/rpc_channel.h"
#include "social/social_identity.h"
#include "social/message_sniffer.h"
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
#include <boost/graph/bellman_ford_shortest_paths.hpp>

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

    // Include all node metadata from input topology in the output graph
    mergeInputNodeMetadata<Graph, Tags::NodeName>(api, graph, properties);
    
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

    // Include all node metadata from input topology in the output graph
    mergeInputNodeMetadata<Graph, Tags::NodeName>(api, graph, properties);
    
    outputGraphDataset(graph, properties, api.getOutputFilename("rt-topo", "graphml"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(DumpRoutingTopology, "state/routing_topology")

class PairWisePing : public TestCase
{
public:
  /// Shortest path type
  typedef std::vector<std::string> ShortestPath;

  /// Dependent routing topology dump
  DumpRoutingTopologyPtr rt_topology;
  /// Raw measurements
  DataSet<> ds_raw{"ds_raw"};
  /// Stretch measurements
  DataSet<ShortestPath> ds_stretch{"ds_stretch"};
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

    typedef typename CompactRoutingTable::TopologyDumpGraph Graph;
    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
    typedef boost::property_map<Graph, boost::vertex_index_t>::type IndexMap;
    typedef boost::property_map<Graph, CompactRoutingTable::TopologyDumpTags::NodeName>::type NameMap;
    typedef boost::iterator_property_map<Vertex*, IndexMap, Vertex, Vertex&> PredecessorMap;
    typedef boost::iterator_property_map<int*, IndexMap, int, int&> DistanceMap;
    std::vector<Vertex> predecessors(boost::num_vertices(topology.graph()));
    std::vector<int> distances(boost::num_vertices(topology.graph()));
    
    IndexMap indexMap = boost::get(boost::vertex_index, topology.graph());
    PredecessorMap predecessorMap(&predecessors[0], indexMap);
    DistanceMap distanceMap(&distances[0], indexMap);
    NameMap nameMap = boost::get(CompactRoutingTable::TopologyDumpTags::NodeName(), topology.graph());

    // Compute path stretches for each raw measurement pair
    for (const auto &record : ds_raw) {
      if (!boost::get<bool>(record.at("success")))
        continue;

      std::string nodeA = boost::get<std::string>(record.at("node_a"));
      std::string nodeB = boost::get<std::string>(record.at("node_b"));
      // TODO: We should group measurements by root vertex to avoid doing the same computation over and over
      boost::bellman_ford_shortest_paths(topology.graph(),
        boost::weight_map(boost::get(CompactRoutingTable::TopologyDumpTags::LinkWeight(), topology.graph()))
        .predecessor_map(predecessorMap)
        .distance_map(distanceMap)
        .root_vertex(topology.vertex(nodeA))
      );

      int measuredLength = boost::get<int>(record.at("hops"));
      int shortestLength = distanceMap[topology.vertex(nodeB)];
      double stretch = (double) measuredLength / (double) shortestLength;

      ShortestPath path;
      Vertex v = topology.vertex(nodeB);
      path.push_back(nameMap[v]);
      for (Vertex u = predecessorMap[v]; u != v; v = u, u = predecessorMap[v]) {
        path.push_back(nameMap[u]);
      }

      ds_stretch.add({
        { "timestamp", record.at("timestamp") },
        { "node_a", nodeA },
        { "node_b", nodeB },
        { "measured", measuredLength },
        { "shortest", shortestLength },
        { "stretch", stretch },
        { "shortest_path", path }
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
    bool sybilMode = argument<bool>("sybil_mode", false);
    bool consistent = true;
    size_t checkedRecords = 0;
    size_t inconsistentRecords = 0;
    for (const auto &record : ds_groups) {
      std::string nodeStringId = boost::get<std::string>(record.at("node_id"));
      NodeIdentifier nodeId(nodeStringId, NodeIdentifier::Format::Hex);
      const Partition::Node &node = api.getNodeById(nodeId);
      size_t groupPrefixLen = boost::get<size_t>(record.at("group_len"));
      NodeIdentifier groupPrefix = nodeId.prefix(groupPrefixLen);
      bool sybilRecord = static_cast<bool>(node.property<int>("sybil"));

      for (const auto &sibling : ds_groups) {
        std::string siblingStringId = boost::get<std::string>(sibling.at("node_id"));
        NodeIdentifier siblingId(siblingStringId, NodeIdentifier::Format::Hex);
        const Partition::Node &siblingNode = api.getNodeById(siblingId);
        bool sybilNode = static_cast<bool>(siblingNode.property<int>("sybil"));

        if (nodeId == siblingId)
          continue;

        if (siblingId.prefix(groupPrefixLen) != groupPrefix)
          continue;

        // Ensure that this node has our record
        checkedRecords++;
        if (!globalNdb[siblingStringId].count(nodeStringId)) {
          if (sybilMode && (sybilRecord || sybilNode))
            continue;

          BOOST_LOG_SEV(logger(), log::error) << "NDB inconsistent, node " 
            << siblingStringId << " (" << siblingNode.name << ") misses record for "
            << nodeStringId << " (" << node.name << ").";
          consistent = false;
          inconsistentRecords++;
        }
      }
    }

    if (!consistent)
      BOOST_LOG_SEV(logger(), log::error) << "NDB inconsistent after checking " << checkedRecords << " records.";
    else
      BOOST_LOG(logger()) << "NDB consistent after checking " << checkedRecords << " records.";

    // Save the fraction of inconsistent records
    DataSet<> ds_report;
    ds_report.add({
      { "checked",  checkedRecords },
      { "failed",   inconsistentRecords },
      { "ratio",    static_cast<double>(checkedRecords - inconsistentRecords) / checkedRecords }
    });

    outputCsvDataset(
      ds_report,
      { "checked", "failed", "ratio" },
      api.getOutputFilename("report", "csv")
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(NdbConsistentSanityCheck, "sanity/check_consistent_ndb")

class GetPerformanceStatistics : public TestCase
{
public:
  /// General statistics dataset
  DataSet<> ds_stats{"ds_stats"};

  using TestCase::TestCase;

  void extractStatistics(TestCaseApi &api,
                         VirtualNodePtr node)
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
      { "rt_lnd",       statsRouter.msgsLandmarkRouted },
      { "ndb_inserts",  statsNdb.recordInsertions },
      { "ndb_updates",  statsNdb.recordUpdates },
      { "ndb_exp",      statsNdb.recordExpirations },
      { "ndb_drops",    statsNdb.recordDrops },
      { "ndb_refresh",  statsNdb.localRefreshes },
      { "sg_msgs",      statsSg.recordXmits },
      { "sg_msgs_r",    statsSg.recordRcvd },
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
  }

  /**
   * Gather some statistics.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    extractStatistics(api, node);
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    api.send(ds_stats);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_stats);

    outputCsvDataset(
      ds_stats,
      {
        "ts", "node_id",
        "rt_msgs", "rt_updates", "rt_exp", "rt_lnd",
        "ndb_inserts", "ndb_updates", "ndb_exp", "ndb_drops", "ndb_refresh",
        "sg_msgs", "sg_msgs_r",
        "rt_s_all", "rt_s_act", "rt_s_vic",
        "ndb_s_all", "ndb_s_act", "ndb_s_cac"
      },
      api.getOutputFilename("raw", "csv", argument<std::string>("marker"))
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
    extractStatistics(api, node);
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

class CollectLinkCongestion : public TestCase
{
public:
  /// Message sniffer
  MessageSniffer sniffer;
  /// Mutex
  std::recursive_mutex mutex;
  /// Link congestion
  std::unordered_map<std::tuple<NodeIdentifier, NodeIdentifier>, size_t> congestion;
  /// Link congestion dataset
  DataSet<> ds_links{"ds_links"};
  /// Results of the pair-wise ping test if one wants congestion stretch computation
  boost::shared_ptr<PairWisePing> pairWisePing;
  /// Shortest path simulated congestion dataset
  DataSet<> ds_spcongestion{"ds_spcongestion"};

  using TestCase::TestCase;

  template <typename T>
  std::tuple<T, T> getEdgeId(const T &a, const T &b)
  {
    // Ensure that edge identifiers are unique regardless of the edge direction
    return a < b ? std::make_tuple(a, b) : std::make_tuple(b, a);
  }

  bool filter(const RoutedMessage &msg)
  {
    return msg.sourceCompId() == static_cast<std::uint32_t>(CompactRouter::Component::RPC_Engine);
  }

  void collect(CompactRouter &router, const RoutedMessage &msg)
  {
    // Skip locally-generated messages
    if (msg.originLinkId().isNull())
      return;

    RecursiveUniqueLock lock(mutex);
    congestion[getEdgeId(router.identity().localId(), msg.originLinkId())]++;
  }

  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    sniffer.attach(*node->router);
  }

  void localNodesRunning(TestCaseApi &api)
  {
    sniffer.signalMatchedMessage.connect(boost::bind(&CollectLinkCongestion::collect, this, _1, _2));
    sniffer.setFilter(boost::bind(&CollectLinkCongestion::filter, this, _1));
    sniffer.start();
  }

  void signalReceived(TestCaseApi &api,
                      const std::string &signal)
  {
    sniffer.stop();
    // Finish the test case as soon as a signal is received
    finish(api);
  }

  void processLocalResults(TestCaseApi &api)
  {
    for (const auto &p : congestion) {
      ds_links.add({
        { "ts",       static_cast<int>(api.getTime()) },
        { "node_id",  std::get<0>(p.first).hex() },
        { "link_id",  std::get<1>(p.first).hex() },
        { "msgs",     p.second }
      });
    }
    api.send(ds_links);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.receive(ds_links);

    outputCsvDataset(
      ds_links,
      { "ts", "node_id", "link_id", "msgs" },
      api.getOutputFilename("raw", "csv", argument<std::string>("marker"))
    );

    if (pairWisePing && pairWisePing->isFinished()) {
      // Compute expected congestion in shortest-path protocols and compare
      std::unordered_map<std::tuple<std::string, std::string>, size_t> spCongestion;

      auto &pairs = pairWisePing->ds_stretch;
      for (const auto &record : pairs) {
        const auto &path = boost::get<PairWisePing::ShortestPath>(record.at("shortest_path"));
        // Make sure to skip the last (source) vertex; this computation should be consistent with
        // the above measurements of real congestion
        for (int i = 0; i < path.size() - 1; i++) {
          // Increase usage by two since each edge is used twice when doing pings (round-trip)
          spCongestion[getEdgeId(path.at(i), path.at(i + 1))] += 2;
        }
      }

      for (const auto &p : spCongestion) {
        ds_spcongestion.add({
          { "node_id", std::get<0>(p.first) },
          { "link_id", std::get<1>(p.first) },
          { "msgs", p.second }
        });
      }
    }

    outputCsvDataset(
      ds_spcongestion,
      { "node_id", "link_id", "msgs" },
      api.getOutputFilename("sp", "csv", argument<std::string>("marker"))
    );
  }
};

UNISPHERE_REGISTER_TEST_CASE(CollectLinkCongestion, "stats/collect_link_congestion")

class SetupSybilNodes : public TestCase
{
public:
  /// A set of known Sybil nodes for faster lookup
  std::unordered_set<NodeIdentifier> sybils;
  /// Evilness switch (off by default)
  bool evil = false;
  /// Signal subscriptions
  std::list<boost::signals2::connection> subscriptions;

  using TestCase::TestCase;

  void preSelection(TestCaseApi &api)
  {
    // Prepare a list of all Sybil nodes so they will be able to collude
    boost::property_tree::ptree args;
    for (const Partition &p : api.getPartitions()) {
      for (const Partition::Node &n : p.nodes) {
        if (n.property<int>("sybil"))
          args.add("sybils.node", n.contact.nodeId().hex());
      }
    }

    // Use global arguments to avoid sending the same list once for each node
    api.setGlobalArguments(args);
  }

  SelectedPartition::Node selectNode(const Partition &partition,
                                     const Partition::Node &node,
                                     TestCaseApi &api)
  {
    // Only run this test case on nodes marked as Sybil
    if (!node.property<int>("sybil"))
      return SelectedPartition::Node();

    return SelectedPartition::Node{ node.contact.nodeId() };
  }

  void preRunNodes(TestCaseApi &api,
                   const boost::property_tree::ptree &args)
  {
    for (const auto &p : args.get_child("sybils", boost::property_tree::ptree())) {
      sybils.insert(NodeIdentifier(p.second.data(), NodeIdentifier::Format::Hex));
    }

    BOOST_LOG(logger()) << "I know of " << sybils.size() << " Sybil nodes.";
  }

  /**
   * Make a Sybil node evil.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    BOOST_LOG(logger()) << "I am an evil Sybil node: " << node->name;

    subscriptions << node->router->nameDb().signalImportRecord.connect(
      [this](NameRecordPtr record) -> bool {
        // Drop any record that doesn't belong to another Sybil node
        return evil ? sybils.count(record->nodeId) != 0 : true;
      }
    );
  }

  void signalReceived(TestCaseApi &api,
                      const std::string &signal)
  {
    if (signal == "finish") {
      // Finish the test case
      for (auto &c : subscriptions)
        c.disconnect();
      subscriptions.clear();

      finish(api);
    } else if (signal == "evil") {
      // Instruct Sybil nodes to become evil
      BOOST_LOG(logger()) << "Sybil nodes becoming evil.";
      evil = true;
    } else if (signal == "nice") {
      // Instruct Sybil nodes to become nice
      BOOST_LOG(logger()) << "Sybil nodes becoming nice.";
      evil = false;
    }
  }
};

UNISPHERE_REGISTER_TEST_CASE(SetupSybilNodes, "roles/setup_sybil_nodes")

}
