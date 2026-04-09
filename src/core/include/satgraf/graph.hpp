#pragma once

#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/clause.hpp"
#include "satgraf/types.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>

namespace satgraf::graph {

template<typename NodeT, typename EdgeT>
class Graph {
public:
    using NodeMap = std::unordered_map<NodeId, NodeT>;
    using EdgeMap = std::unordered_map<EdgeId, EdgeT>;

    Graph() = default;

    NodeT& createNode(NodeId id, std::string name) {
        auto [it, inserted] = nodes_.emplace(id, NodeT{id, std::move(name)});
        return it->second;
    }

    EdgeT& createEdge(NodeId from, NodeId to) {
        EdgeId eid{next_edge_id_++};
        auto [it, inserted] = edges_.emplace(eid, EdgeT{eid, from, to});
        return it->second;
    }

    void connect(EdgeId eid, NodeId node) {
        auto edge_it = edges_.find(eid);
        if (edge_it == edges_.end()) return;
        auto node_it = nodes_.find(node);
        if (node_it == nodes_.end()) return;

        node_it->second.edges.push_back(eid);

        const EdgeT& edge = edge_it->second;
        if (edge.source == node) {
            auto target_it = nodes_.find(edge.target);
            if (target_it != nodes_.end()) {
                target_it->second.edges.push_back(eid);
            }
        } else if (edge.target == node) {
            auto source_it = nodes_.find(edge.source);
            if (source_it != nodes_.end()) {
                source_it->second.edges.push_back(eid);
            }
        }
    }

    std::optional<std::reference_wrapper<NodeT>> getNode(NodeId id) {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) return std::ref(it->second);
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const NodeT>> getNode(NodeId id) const {
        auto it = nodes_.find(id);
        if (it != nodes_.end()) return std::cref(it->second);
        return std::nullopt;
    }

    NodeMap& getNodes() { return nodes_; }
    const NodeMap& getNodes() const { return nodes_; }

    EdgeMap& getEdges() { return edges_; }
    const EdgeMap& getEdges() const { return edges_; }

    std::vector<Clause>& getClauses() { return clauses_; }
    const std::vector<Clause>& getClauses() const { return clauses_; }

    void removeNode(NodeId id) {
        auto it = nodes_.find(id);
        if (it == nodes_.end()) return;

        const auto& node = it->second;
        auto node_edges = node.edges;

        for (EdgeId eid : node_edges) {
            removeEdge(eid);
        }

        nodes_.erase(it);
    }

    void removeEdge(EdgeId id) {
        auto it = edges_.find(id);
        if (it == edges_.end()) return;

        const EdgeT& edge = it->second;

        auto src_it = nodes_.find(edge.source);
        if (src_it != nodes_.end()) {
            auto& src_edges = src_it->second.edges;
            src_edges.erase(
                std::remove(src_edges.begin(), src_edges.end(), id),
                src_edges.end());
        }

        auto tgt_it = nodes_.find(edge.target);
        if (tgt_it != nodes_.end()) {
            auto& tgt_edges = tgt_it->second.edges;
            tgt_edges.erase(
                std::remove(tgt_edges.begin(), tgt_edges.end(), id),
                tgt_edges.end());
        }

        edges_.erase(it);
    }

    Clause& addClause() {
        clauses_.emplace_back();
        return clauses_.back();
    }

    size_t nodeCount() const noexcept { return nodes_.size(); }
    size_t edgeCount() const noexcept { return edges_.size(); }

private:
    NodeMap nodes_;
    EdgeMap edges_;
    std::vector<Clause> clauses_;
    uint32_t next_edge_id_{0};
};

} // namespace satgraf::graph
