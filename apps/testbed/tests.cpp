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
#include "core/operators.h"
#include "testbed/test_bed.h"
#include "testbed/dataset/graphs.hpp"
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
#include <unordered_set>
#include <boost/range/adaptors.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>

using namespace UniSphere;
using namespace UniSphere::TestBed;

namespace Tests {

class DumpSloppyGroupTopology : public TestCase
{
public:
  /// Graph topology type
  typedef SloppyGroupManager::TopologyDumpGraph Graph;
  /// Graph storage
  Graph graph;

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
    api.dataset("ds_topology").add()
      ("graph", graph.graph())
    ;
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = SloppyGroupManager::TopologyDumpTags;

    mergeGraphDataset<Graph, Tags::NodeName, Tags::Placeholder>(api.dataset("ds_topology"), "graph", graph);

    BOOST_LOG(logger()) << "Received " << boost::num_vertices(graph.graph()) << " vertices in ds_topology (after merge).";

    boost::dynamic_properties properties;
    properties.property("name", boost::get(Tags::NodeName(), graph.graph()));
    properties.property("group", boost::get(Tags::NodeGroup(), graph.graph()));
    properties.property("group_plen", boost::get(Tags::NodeGroupPrefixLength(), graph.graph()));
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
    api.dataset("ds_topology").add()
      ("graph", graph.graph())
    ;
  }

  void processGlobalResults(TestCaseApi &api)
  {
    using Tags = CompactRoutingTable::TopologyDumpTags;

    mergeGraphDataset<Graph, Tags::NodeName, Tags::Placeholder>(api.dataset("ds_topology"), "graph", graph);

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
    bool sybilMode = argument<bool>("sybil_mode", false);
    bool communityLimit = argument<bool>("community_limit", false);
    // In case sybil mode is enabled, we should not perform measurements from sybil nodes
    if (sybilMode && node.property<int>("sybil"))
      return SelectedPartition::Node();

    boost::property_tree::ptree args;
    auto nodes = api.getNodes();
    int m = argument<int>("destinations_per_node", 1);
    std::set<int> indices;
    std::set<int> excluded;

    // Discover the number of nodes (iterator doesn't support random access) and
    // any nodes that should be excluded
    int n = 0;
    int indexSelf = -1;
    for (const Partition::Node &pnode : nodes) {
      // In case sybil mode is enabled, we should not perform measurements to sybil nodes
      if (sybilMode && pnode.property<int>("sybil"))
        excluded.insert(n);
      // In case community limit is enabled, we should not perform measurements to nodes
      // that are in a different community that the selected node
      if (communityLimit && pnode.property<std::string>("community") != node.property<std::string>("community"))
        excluded.insert(n);
      // Exclude ourselves as this would serve no purpuse
      if (pnode.contact.nodeId() == node.contact.nodeId())
        excluded.insert(n);

      n++;
    }

    // Ensure that the index range will be valid
    if (m > n)
      m = n;

    // Draw m random indices to select which nodes to choose
    std::uniform_int_distribution<int> sampler(0, n - 1);
    for (int i = 0; i < m; i++) {
      for (;;) {
        int idx = sampler(api.rng());
        if (excluded.count(idx))
          continue;
        if (indices.insert(idx).second)
          break;
      }
    }

    // Use selected indices to populate the argument list with destination ids
    auto it = nodes.begin();
    int offset = 0;
    for (int idx : indices) {
      std::advance(it, idx - offset);
      offset = idx;

      args.add("nodes.node", it->contact.nodeId().hex());
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
            api.dataset("ds_raw").add()
              ("timestamp", boost::posix_time::microsec_clock::universal_time())
              ("node_a", node->nodeId)
              ("node_b", nodeId)
              ("success", true)
#ifdef UNISPHERE_PROFILE
              ("msg_id", node->router->msgTracer().getMessageId(msg))
#endif
              ("hops", msg.hopDistance())
              ("rtt", duration_cast<microseconds>(high_resolution_clock::now() - xmitTime).count())
            ;
            callNext(api);
          },
          [this, &api, node, nodeId](RpcErrorCode code, const std::string &msg) {
            api.dataset("ds_raw").add()
              ("timestamp", boost::posix_time::microsec_clock::universal_time())
              ("node_a", node->nodeId)
              ("node_b", nodeId)
              ("success", false)
            ;
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
  }

  void processGlobalResults(TestCaseApi &api)
  {
    auto ds_raw = api.dataset("ds_raw");
    auto ds_stretch = api.dataset("ds_stretch");

    // Output RAW dataset received from slaves
    ds_raw.csv(
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
      if (!record.field<bool>("success"))
        continue;

      std::string nodeA = record.field<std::string>("node_a");
      std::string nodeB = record.field<std::string>("node_b");
      // TODO: We should group measurements by root vertex to avoid doing the same computation over and over
      boost::bellman_ford_shortest_paths(topology.graph(),
        boost::weight_map(boost::get(CompactRoutingTable::TopologyDumpTags::LinkWeight(), topology.graph()))
        .predecessor_map(predecessorMap)
        .distance_map(distanceMap)
        .root_vertex(topology.vertex(nodeA))
      );

      int measuredLength = record.field<int>("hops");
      int shortestLength = distanceMap[topology.vertex(nodeB)];
      double stretch = (double) measuredLength / (double) shortestLength;

      ShortestPath path;
      Vertex v = topology.vertex(nodeB);
      path.push_back(nameMap[v]);
      for (Vertex u = predecessorMap[v]; u != v; v = u, u = predecessorMap[v]) {
        path.push_back(nameMap[u]);
      }

      ds_stretch.add()
        ("timestamp", record.field<boost::posix_time::ptime>("timestamp"))
        ("node_a", nodeA)
        ("node_b", nodeB)
        ("measured", measuredLength)
        ("shortest", shortestLength)
        ("stretch", stretch)
        ("shortest_path", path)
      ;
    }

    ds_stretch.csv(
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
  using TestCase::TestCase;

  /**
   * Retrieve packet traces.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
#ifdef UNISPHERE_PROFILE
    DataSet ds_traces = api.dataset("ds_traces");
    for (const auto &p : node->router->msgTracer().getTraceRecords()) {
      ds_traces.add()
        ("node_id",        node->nodeId)
        ("pkt_id",         p.first)
        ("timestamp",      p.second.at("timestamp"))
        ("src",            p.second.at("src"))
        ("dst",            p.second.at("dst"))
        ("dst_lr",         p.second.at("dst_lr"))
        ("route_duration", p.second.at("route_duration"))
        ("local",          p.second.at("local"))
        ("processed",      p.second.at("processed"))
      ;
    }
#endif

    finish(api);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.dataset("ds_traces").csv(
      { "node_id", "pkt_id", "timestamp", "src", "dst", "dst_lr", "route_duration", "local", "processed" },
      api.getOutputFilename("traces", "csv")
    ).clear();
  }
};

UNISPHERE_REGISTER_TEST_CASE(GetMessageTraces, "traces/retrieve")

class NdbConsistentSanityCheck : public TestCase
{
public:
  using TestCase::TestCase;

  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    api.dataset("ds_groups").add()
      ("node_id",    node->nodeId)
      ("group_len",  node->router->sloppyGroup().getGroupPrefixLength())
    ;

    DataSet ds_ndb = api.dataset("ds_ndb");
    for (NameRecordPtr record : node->router->nameDb().getNames(NameRecord::Type::SloppyGroup)) {
      ds_ndb.add()
        ("node_id",    node->nodeId)
        ("record_id",  record->nodeId)
        ("ts",         record->timestamp)
        ("seqno",      record->seqno)
      ;
    }
    finish(api);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    DataSet ds_ndb = api.dataset("ds_ndb");
    DataSet ds_groups = api.dataset("ds_groups");

    ds_ndb.csv({ "node_id", "record_id", "ts", "seqno" }, api.getOutputFilename("raw", "csv"));

    // Build a per-node map of name records
    std::unordered_map<std::string, std::unordered_set<std::string>> globalNdb;
    for (const auto &record : ds_ndb) {
      globalNdb[record.field<std::string>("node_id")].insert(record.field<std::string>("record_id"));
    }

    // Check record consistency
    bool sybilMode = argument<bool>("sybil_mode", false);
    bool consistent = true;
    size_t checkedRecords = 0;
    size_t inconsistentRecords = 0;
    for (const auto &record : ds_groups) {
      std::string nodeStringId = record.field<std::string>("node_id");
      NodeIdentifier nodeId(nodeStringId, NodeIdentifier::Format::Hex);
      const Partition::Node &node = api.getNodeById(nodeId);
      size_t groupPrefixLen = record.field<int>("group_len");
      NodeIdentifier groupPrefix = nodeId.prefix(groupPrefixLen);
      bool sybilRecord = static_cast<bool>(node.property<int>("sybil"));

      for (const auto &sibling : ds_groups) {
        std::string siblingStringId = sibling.field<std::string>("node_id");
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
    auto ds_report = api.dataset("ds_report");
    ds_report.add()
      ("checked",  checkedRecords)
      ("failed",   inconsistentRecords)
      ("ratio",    static_cast<double>(checkedRecords - inconsistentRecords) / checkedRecords)
    ;

    ds_report.csv({ "checked", "failed", "ratio" }, api.getOutputFilename("report", "csv"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(NdbConsistentSanityCheck, "sanity/check_consistent_ndb")

class GetPerformanceStatistics : public TestCase
{
public:
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

    api.dataset("ds_stats").add()
      // Timestamp and node identifier
      ("ts",            api.getTime())
      ("node_id",       node->nodeId)
      // Messaging complexity
      ("rt_msgs",      statsRouter.entryXmits)
      ("rt_updates",   statsRt.routeUpdates)
      ("rt_exp",       statsRt.routeExpirations)
      ("rt_lnd",       statsRouter.msgsLandmarkRouted)
      ("sa_msgs",      statsRouter.saUpdateXmits)
      ("ndb_inserts",  statsNdb.recordInsertions)
      ("ndb_updates",  statsNdb.recordUpdates)
      ("ndb_exp",      statsNdb.recordExpirations)
      ("ndb_drops",    statsNdb.recordDrops)
      ("ndb_refresh",  statsNdb.localRefreshes)
      ("sg_msgs",      statsSg.recordXmits)
      ("sg_msgs_r",    statsSg.recordRcvd)
      ("lm_sent",      statsLink.global.msgXmits)
      ("lm_rcvd",      statsLink.global.msgRcvd)
      // Local state complexity
      //   Routing table
      ("rt_s_all",     rt.size())
      ("rt_s_act",     rt.sizeActive())
      ("rt_s_vic",     rt.sizeVicinity())
      //   Name database
      ("ndb_s_all",    ndb.size())
      ("ndb_s_act",    ndb.sizeActive())
      ("ndb_s_cac",    ndb.sizeCache())
    ;
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

  void processGlobalResults(TestCaseApi &api)
  {
    api.dataset("ds_stats").csv(
      {
        "ts", "node_id",
        "rt_msgs", "rt_updates", "rt_exp", "rt_lnd",
        "sa_msgs",
        "ndb_inserts", "ndb_updates", "ndb_exp", "ndb_drops", "ndb_refresh",
        "sg_msgs", "sg_msgs_r",
        "rt_s_all", "rt_s_act", "rt_s_vic",
        "ndb_s_all", "ndb_s_act", "ndb_s_cac"
      },
      api.getOutputFilename("raw", "csv", argument<std::string>("marker"))
    ).clear();
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
  /// Results of the pair-wise ping test if one wants congestion stretch computation
  boost::shared_ptr<PairWisePing> pairWisePing;

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
    DataSet ds_links = api.dataset("ds_links");
    for (const auto &p : congestion) {
      ds_links.add()
        ("ts",        api.getTime())
        ("node_id",   std::get<0>(p.first))
        ("link_id",   std::get<1>(p.first))
        ("msgs",      p.second)
      ;
    }
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.dataset("ds_links").csv(
      { "ts", "node_id", "link_id", "msgs" },
      api.getOutputFilename("raw", "csv", argument<std::string>("marker"))
    ).clear();

    if (pairWisePing && pairWisePing->isFinished()) {
      DataSet ds_spcongestion = api.dataset("ds_spcongestion");
      // Compute expected congestion in shortest-path protocols and compare
      std::unordered_map<std::tuple<std::string, std::string>, size_t> spCongestion;

      for (const auto &record : api.dataset(pairWisePing, "ds_stretch")) {
        const auto &path = record.field<PairWisePing::ShortestPath>("shortest_path");
        // Make sure to skip the last (source) vertex; this computation should be consistent with
        // the above measurements of real congestion
        for (int i = 0; i < path.size() - 1; i++) {
          // Increase usage by two since each edge is used twice when doing pings (round-trip)
          spCongestion[getEdgeId(path.at(i), path.at(i + 1))] += 2;
        }
      }

      for (const auto &p : spCongestion) {
        ds_spcongestion.add()
          ("node_id", std::get<0>(p.first))
          ("link_id", std::get<1>(p.first))
          ("msgs",    p.second)
        ;
      }

      ds_spcongestion.csv(
        { "node_id", "link_id", "msgs" },
        api.getOutputFilename("sp", "csv", argument<std::string>("marker"))
      ).clear();
    }
  }
};

UNISPHERE_REGISTER_TEST_CASE(CollectLinkCongestion, "stats/collect_link_congestion")

class GetLRAddressLengths : public TestCase
{
public:
  using TestCase::TestCase;

  /**
   * Gather some statistics.
   */
  void runNode(TestCaseApi &api,
               VirtualNodePtr node,
               const boost::property_tree::ptree &args)
  {
    DataSet ds_primary = api.dataset("ds_primary");
    DataSet ds_secondary = api.dataset("ds_secondary");

    bool primary = true;
    for (const LandmarkAddress &address : node->router->routingTable().getLocalAddresses()) {
      auto &ds = primary ? ds_primary : ds_secondary;
      primary = false;
      ds.add()
        ("node_id",  node->nodeId)
        ("length",   address.size())
      ;
    }
    finish(api);
  }

  void processGlobalResults(TestCaseApi &api)
  {
    api.dataset("ds_primary").csv({ "node_id", "length" }, api.getOutputFilename("primary", "csv"));
    api.dataset("ds_secondary").csv({ "node_id", "length" }, api.getOutputFilename("secondary", "csv"));
  }
};

UNISPHERE_REGISTER_TEST_CASE(GetLRAddressLengths, "stats/lr_address_lengths")

class SetupSybilNodes : public TestCase
{
public:
  /// A set of known Sybil nodes for faster lookup
  std::unordered_set<NodeIdentifier> sybils;
  /// Evilness switch for name records (off by default)
  bool evilNames = false;
  /// Evilness switch for data forwarding (off by default)
  bool evilRouting = false;
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
        return evilNames ? sybils.count(record->nodeId) != 0 : true;
      }
    );

    subscriptions << node->router->signalForwardMessage.connect(
      [this](const RoutedMessage &msg) -> bool {
        // Drop any message not sent by a Sybil node
        return evilRouting ? sybils.count(msg.sourceNodeId()) != 0 : true;
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
    } else if (signal == "evil_names") {
      // Instruct Sybil nodes to become evil
      BOOST_LOG(logger()) << "Sybil nodes becoming evil (names).";
      evilNames = true;
    } else if (signal == "nice_names") {
      // Instruct Sybil nodes to become nice
      BOOST_LOG(logger()) << "Sybil nodes becoming nice (names).";
      evilNames = false;
    } else if (signal == "evil_routing") {
      // Instruct Sybil nodes to become evil
      BOOST_LOG(logger()) << "Sybil nodes becoming evil (routing).";
      evilRouting = true;
    } else if (signal == "nice_routing") {
      // Instruct Sybil nodes to become nice
      BOOST_LOG(logger()) << "Sybil nodes becoming nice (routing).";
      evilRouting = false;
    }
  }
};

UNISPHERE_REGISTER_TEST_CASE(SetupSybilNodes, "roles/setup_sybil_nodes")

}
