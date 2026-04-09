#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/types.hpp"
#include "satgraf/union_find.hpp"

#include <igraph.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace satgraf::community {

struct CommunityResult {
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment;
    double q_modularity{0.0};

    size_t min_community_size{0};
    size_t max_community_size{0};
    double mean_community_size{0.0};
    double sd_community_size{0.0};

    size_t inter_edges{0};
    size_t intra_edges{0};
    double edge_ratio{0.0};
};

class Detector {
public:
    virtual ~Detector() = default;
    virtual CommunityResult detect(
        const graph::Graph<graph::Node, graph::Edge>& graph) = 0;
    virtual std::string name() const = 0;
};

namespace detail {

struct IgraphPtr {
    igraph_t ig{};
    bool owns{false};
    IgraphPtr() = default;
    ~IgraphPtr() { if (owns) igraph_destroy(&ig); }
    IgraphPtr(IgraphPtr&& o) noexcept : ig(o.ig), owns(o.owns) { o.owns = false; }
    IgraphPtr& operator=(IgraphPtr&& o) noexcept {
        if (this != &o) {
            if (owns) igraph_destroy(&ig);
            ig = o.ig; owns = o.owns; o.owns = false;
        }
        return *this;
    }
    IgraphPtr(const IgraphPtr&) = delete;
    IgraphPtr& operator=(const IgraphPtr&) = delete;
};

struct VectorIntPtr {
    igraph_vector_int_t v{};
    bool owns{false};
    VectorIntPtr() { igraph_vector_int_init(&v, 0); owns = true; }
    ~VectorIntPtr() { if (owns) igraph_vector_int_destroy(&v); }
    VectorIntPtr(VectorIntPtr&& o) noexcept : v(o.v), owns(o.owns) { o.owns = false; }
    VectorIntPtr& operator=(VectorIntPtr&& o) noexcept {
        if (this != &o) {
            if (owns) igraph_vector_int_destroy(&v);
            v = o.v; owns = o.owns; o.owns = false;
        }
        return *this;
    }
    VectorIntPtr(const VectorIntPtr&) = delete;
    VectorIntPtr& operator=(const VectorIntPtr&) = delete;
};

struct VectorPtr {
    igraph_vector_t v{};
    bool owns{false};
    VectorPtr() { igraph_vector_init(&v, 0); owns = true; }
    ~VectorPtr() { if (owns) igraph_vector_destroy(&v); }
    VectorPtr(VectorPtr&& o) noexcept : v(o.v), owns(o.owns) { o.owns = false; }
    VectorPtr& operator=(VectorPtr&& o) noexcept {
        if (this != &o) {
            if (owns) igraph_vector_destroy(&v);
            v = o.v; owns = o.owns; o.owns = false;
        }
        return *this;
    }
    VectorPtr(const VectorPtr&) = delete;
    VectorPtr& operator=(const VectorPtr&) = delete;
};

struct MatrixIntPtr {
    igraph_matrix_int_t m{};
    bool owns{false};
    MatrixIntPtr() { igraph_matrix_int_init(&m, 0, 0); owns = true; }
    ~MatrixIntPtr() { if (owns) igraph_matrix_int_destroy(&m); }
    MatrixIntPtr(MatrixIntPtr&& o) noexcept : m(o.m), owns(o.owns) { o.owns = false; }
    MatrixIntPtr& operator=(MatrixIntPtr&& o) noexcept {
        if (this != &o) {
            if (owns) igraph_matrix_int_destroy(&m);
            m = o.m; owns = o.owns; o.owns = false;
        }
        return *this;
    }
    MatrixIntPtr(const MatrixIntPtr&) = delete;
    MatrixIntPtr& operator=(const MatrixIntPtr&) = delete;
};

struct IgraphConversion {
    IgraphPtr igraph_handle;
    std::vector<graph::NodeId> node_ids;
    std::unordered_map<graph::NodeId, igraph_integer_t> node_to_idx;
};

inline IgraphConversion graph_to_igraph(
    const graph::Graph<graph::Node, graph::Edge>& g)
{
    IgraphConversion conv;

    const auto& nodes = g.getNodes();
    const auto& edges = g.getEdges();

    conv.node_ids.reserve(nodes.size());
    for (const auto& [nid, node] : nodes) {
        (void)node;
        conv.node_ids.push_back(nid);
    }
    std::sort(conv.node_ids.begin(), conv.node_ids.end());

    for (size_t i = 0; i < conv.node_ids.size(); ++i) {
        conv.node_to_idx[conv.node_ids[i]] =
            static_cast<igraph_integer_t>(i);
    }

    const igraph_integer_t n =
        static_cast<igraph_integer_t>(conv.node_ids.size());
    igraph_empty(&conv.igraph_handle.ig, n, IGRAPH_UNDIRECTED);
    conv.igraph_handle.owns = true;

    VectorIntPtr ig_edges;
    igraph_vector_int_reserve(&ig_edges.v,
                              static_cast<igraph_integer_t>(edges.size() * 2));

    for (const auto& [eid, edge] : edges) {
        (void)eid;
        auto src_it = conv.node_to_idx.find(edge.source);
        auto tgt_it = conv.node_to_idx.find(edge.target);
        if (src_it == conv.node_to_idx.end() ||
            tgt_it == conv.node_to_idx.end()) {
            continue;
        }
        igraph_vector_int_push_back(&ig_edges.v, src_it->second);
        igraph_vector_int_push_back(&ig_edges.v, tgt_it->second);
    }

    igraph_add_edges(&conv.igraph_handle.ig, &ig_edges.v, nullptr);

    return conv;
}

inline bool is_graph_connected(
    const graph::Graph<graph::Node, graph::Edge>& graph)
{
    const auto& nodes = graph.getNodes();
    if (nodes.size() <= 1) return true;

    std::vector<graph::NodeId> sorted_ids;
    sorted_ids.reserve(nodes.size());
    for (const auto& [nid, node] : nodes) {
        (void)node;
        sorted_ids.push_back(nid);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());

    std::unordered_map<graph::NodeId, size_t> id_to_idx;
    for (size_t i = 0; i < sorted_ids.size(); ++i) {
        id_to_idx[sorted_ids[i]] = i;
    }

    UnionFind uf(nodes.size());
    for (const auto& [eid, edge] : graph.getEdges()) {
        (void)eid;
        auto src_it = id_to_idx.find(edge.source);
        auto tgt_it = id_to_idx.find(edge.target);
        if (src_it == id_to_idx.end() || tgt_it == id_to_idx.end()) continue;
        uf.unite(src_it->second, tgt_it->second);
    }

    return uf.count_components() <= 1;
}

inline void compute_community_stats(CommunityResult& result)
{
    if (result.assignment.empty()) return;

    std::unordered_map<graph::CommunityId, size_t> community_sizes;
    for (const auto& [nid, cid] : result.assignment) {
        (void)nid;
        community_sizes[cid]++;
    }

    if (community_sizes.empty()) return;

    size_t min_s = community_sizes.begin()->second;
    size_t max_s = 0;
    double sum = 0.0;

    for (const auto& [cid, sz] : community_sizes) {
        (void)cid;
        min_s = std::min(min_s, sz);
        max_s = std::max(max_s, sz);
        sum += static_cast<double>(sz);
    }

    const double mean =
        sum / static_cast<double>(community_sizes.size());

    double variance = 0.0;
    for (const auto& [cid, sz] : community_sizes) {
        (void)cid;
        const double diff = static_cast<double>(sz) - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(community_sizes.size());

    result.min_community_size = min_s;
    result.max_community_size = max_s;
    result.mean_community_size = mean;
    result.sd_community_size = std::sqrt(variance);
}

inline void compute_edge_stats(
    CommunityResult& result,
    const graph::Graph<graph::Node, graph::Edge>& graph)
{
    size_t inter = 0;
    size_t intra = 0;

    for (const auto& [eid, edge] : graph.getEdges()) {
        (void)eid;
        auto src_it = result.assignment.find(edge.source);
        auto tgt_it = result.assignment.find(edge.target);
        if (src_it == result.assignment.end() ||
            tgt_it == result.assignment.end()) {
            continue;
        }

        if (src_it->second == tgt_it->second) {
            intra++;
        } else {
            inter++;
        }
    }

    result.inter_edges = inter;
    result.intra_edges = intra;
    result.edge_ratio =
        (intra == 0)
            ? 0.0
            : static_cast<double>(inter) / static_cast<double>(intra);
}

inline double compute_modularity(
    const std::unordered_map<graph::NodeId, graph::CommunityId>& assignment,
    const graph::Graph<graph::Node, graph::Edge>& graph)
{
    const double two_m = static_cast<double>(graph.edgeCount()) * 2.0;
    if (two_m == 0.0) return 0.0;

    double sum_intra = 0.0;
    std::unordered_map<graph::CommunityId, double> community_degree;

    for (const auto& [eid, edge] : graph.getEdges()) {
        (void)eid;
        auto src_it = assignment.find(edge.source);
        auto tgt_it = assignment.find(edge.target);
        if (src_it == assignment.end() || tgt_it == assignment.end()) continue;

        if (src_it->second == tgt_it->second) {
            sum_intra += 1.0;
        }
    }

    for (const auto& [nid, node] : graph.getNodes()) {
        auto assign_it = assignment.find(nid);
        if (assign_it == assignment.end()) continue;
        community_degree[assign_it->second] +=
            static_cast<double>(node.edges.size());
    }

    double sum_expected = 0.0;
    for (const auto& [cid, deg_sum] : community_degree) {
        (void)cid;
        sum_expected += deg_sum * deg_sum;
    }

    // Q = (2*sum_intra - sum_expected / two_m) / two_m
    return (2.0 * sum_intra - sum_expected / two_m) / two_m;
}

inline std::unordered_set<graph::NodeId> compute_bridge_nodes(
    const CommunityResult& result,
    const graph::Graph<graph::Node, graph::Edge>& graph)
{
    std::unordered_set<graph::NodeId> bridge;
    for (const auto& [eid, edge] : graph.getEdges()) {
        (void)eid;
        auto src_it = result.assignment.find(edge.source);
        auto tgt_it = result.assignment.find(edge.target);
        if (src_it == result.assignment.end() ||
            tgt_it == result.assignment.end()) {
            continue;
        }
        if (src_it->second != tgt_it->second) {
            bridge.insert(edge.source);
            bridge.insert(edge.target);
        }
    }
    return bridge;
}

}  // namespace detail

class CNMDetector : public Detector {
public:
    CommunityResult detect(
        const graph::Graph<graph::Node, graph::Edge>& graph) override
    {
        CommunityResult result;
        if (graph.nodeCount() == 0) return result;

        auto conv = detail::graph_to_igraph(graph);

        detail::MatrixIntPtr merges;
        detail::VectorPtr modularity;
        detail::VectorIntPtr membership;

        igraph_community_fastgreedy(&conv.igraph_handle.ig, nullptr,
                                    &merges.m, &modularity.v,
                                    &membership.v);

        const igraph_integer_t mod_len = igraph_vector_size(&modularity.v);
        const igraph_integer_t n = igraph_vcount(&conv.igraph_handle.ig);

        if (mod_len > 0) {
            igraph_integer_t best_index = 0;
            igraph_real_t best_modularity = -std::numeric_limits<igraph_real_t>::infinity();

            for (igraph_integer_t i = 0; i < mod_len; ++i) {
                const igraph_real_t current = VECTOR(modularity.v)[i];
                if (current > best_modularity) {
                    best_modularity = current;
                    best_index = i;
                }
            }

            igraph_vector_int_resize(&membership.v, n);
            igraph_community_to_membership(&merges.m, n, best_index,
                                           &membership.v, nullptr);

            igraph_real_t modularity_value = 0.0;
            igraph_modularity(&conv.igraph_handle.ig, &membership.v, nullptr,
                              1.0, false, &modularity_value);
            result.q_modularity = modularity_value;
        }

        for (igraph_integer_t i = 0; i < n; ++i) {
            const auto cid =
                static_cast<uint32_t>(VECTOR(membership.v)[i]);
            result.assignment[conv.node_ids[static_cast<size_t>(i)]] =
                graph::CommunityId{cid};
        }

        detail::compute_community_stats(result);
        detail::compute_edge_stats(result, graph);

        return result;
    }

    std::string name() const override { return "cnm"; }
};

class LouvainDetector : public Detector {
public:
    explicit LouvainDetector(bool auto_fallback = true)
        : auto_fallback_{auto_fallback} {}

    CommunityResult detect(
        const graph::Graph<graph::Node, graph::Edge>& graph) override
    {
        if (!detail::is_graph_connected(graph)) {
            std::cerr << "[LouvainDetector] Warning: graph is disconnected.\n";
            if (auto_fallback_) {
                std::cerr << "[LouvainDetector] Auto-falling back to CNM.\n";
                CNMDetector cnm;
                return cnm.detect(graph);
            }
        }

        CommunityResult result;
        if (graph.nodeCount() == 0) return result;

        auto conv = detail::graph_to_igraph(graph);

        detail::VectorIntPtr membership;
        detail::VectorPtr modularity;

        igraph_community_multilevel(&conv.igraph_handle.ig, nullptr, 1.0,
                                    &membership.v, nullptr, &modularity.v);

        const igraph_integer_t mod_len = igraph_vector_size(&modularity.v);
        if (mod_len > 0) {
            result.q_modularity = VECTOR(modularity.v)[mod_len - 1];
        }

        const igraph_integer_t n =
            igraph_vcount(&conv.igraph_handle.ig);
        for (igraph_integer_t i = 0; i < n; ++i) {
            const auto cid =
                static_cast<uint32_t>(VECTOR(membership.v)[i]);
            result.assignment[conv.node_ids[static_cast<size_t>(i)]] =
                graph::CommunityId{cid};
        }

        detail::compute_community_stats(result);
        detail::compute_edge_stats(result, graph);

        return result;
    }

    std::string name() const override { return "louvain"; }

private:
    bool auto_fallback_;
};

class OnlineDetector : public Detector {
public:
    CommunityResult detect(
        const graph::Graph<graph::Node, graph::Edge>& graph) override
    {
        CommunityResult result;
        if (graph.nodeCount() == 0) return result;

        std::vector<graph::NodeId> sorted_nodes;
        sorted_nodes.reserve(graph.getNodes().size());
        for (const auto& [nid, node] : graph.getNodes()) {
            (void)node;
            sorted_nodes.push_back(nid);
        }
        std::sort(sorted_nodes.begin(), sorted_nodes.end());

        uint32_t next_community = 0;

        for (const auto& nid : sorted_nodes) {
            std::unordered_map<uint32_t, size_t> community_counts;

            auto node_it = graph.getNodes().find(nid);
            if (node_it == graph.getNodes().end()) continue;
            const auto& node = node_it->second;

            for (const auto& eid : node.edges) {
                auto edge_it = graph.getEdges().find(eid);
                if (edge_it == graph.getEdges().end()) continue;
                const auto& edge = edge_it->second;

                const graph::NodeId neighbor =
                    (edge.source == nid) ? edge.target : edge.source;

                auto neighbor_assign = result.assignment.find(neighbor);
                if (neighbor_assign != result.assignment.end()) {
                    community_counts[static_cast<uint32_t>(
                        neighbor_assign->second)]++;
                }
            }

            if (community_counts.empty()) {
                result.assignment[nid] =
                    graph::CommunityId{next_community++};
            } else {
                uint32_t best_community = community_counts.begin()->first;
                size_t best_count = community_counts.begin()->second;
                for (const auto& [cid, count] : community_counts) {
                    if (count > best_count) {
                        best_count = count;
                        best_community = cid;
                    }
                }
                result.assignment[nid] = graph::CommunityId{best_community};
            }
        }

        result.q_modularity =
            detail::compute_modularity(result.assignment, graph);

        detail::compute_community_stats(result);
        detail::compute_edge_stats(result, graph);

        return result;
    }

    std::string name() const override { return "online"; }
};

class DetectorFactory {
public:
    using Creator = std::function<std::unique_ptr<Detector>()>;

    static DetectorFactory& instance() {
        static DetectorFactory factory;
        return factory;
    }

    void register_detector(const std::string& name, Creator creator) {
        const std::lock_guard<std::mutex> lock(mutex_);
        creators_[name] = std::move(creator);
    }

    std::unique_ptr<Detector> create(const std::string& name) const {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto it = creators_.find(name);
        if (it == creators_.end()) {
            throw std::runtime_error(
                "Unknown community detector: '" + name + "'");
        }
        return it->second();
    }

    std::vector<std::string> available_algorithms() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(creators_.size());
        for (const auto& [name, creator] : creators_) {
            (void)creator;
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

private:
    DetectorFactory() {
        creators_["louvain"] = [] {
            return std::make_unique<LouvainDetector>();
        };
        creators_["cnm"] = [] {
            return std::make_unique<CNMDetector>();
        };
        creators_["online"] = [] {
            return std::make_unique<OnlineDetector>();
        };
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Creator> creators_;
};

}  // namespace satgraf::community
