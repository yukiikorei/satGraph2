#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/types.hpp"

#include <cassert>
#include <vector>

namespace satgraf {

template<typename NodeT, typename EdgeT>
class CSR {
public:
    CSR() = default;

    void build(const graph::Graph<NodeT, EdgeT>& g) {
        row_offsets_.clear();
        column_indices_.clear();
        valid_ = true;

        const auto& nodes = g.getNodes();
        const auto& edges = g.getEdges();
        const size_t n = nodes.size();

        row_offsets_.resize(n + 1, 0);

        std::vector<std::pair<graph::NodeId, const NodeT*>> sorted_nodes;
        sorted_nodes.reserve(n);
        for (const auto& [nid, node] : nodes) {
            sorted_nodes.emplace_back(nid, &node);
        }
        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        for (size_t i = 0; i < sorted_nodes.size(); ++i) {
            row_offsets_[i + 1] = row_offsets_[i] + sorted_nodes[i].second->edges.size();
        }

        column_indices_.reserve(row_offsets_.back());
        for (const auto& [nid, node_ptr] : sorted_nodes) {
            for (const auto eid : node_ptr->edges) {
                auto edge_it = edges.find(eid);
                if (edge_it == edges.end()) continue;
                const auto& edge = edge_it->second;
                if (edge.source == nid) {
                    column_indices_.push_back(edge.target);
                } else {
                    column_indices_.push_back(edge.source);
                }
            }
        }

        node_ids_ = sorted_nodes;
    }

    void invalidate() noexcept { valid_ = false; }
    bool is_valid() const noexcept { return valid_; }

    size_t neighbor_count(size_t node_idx) const {
        assert(node_idx + 1 < row_offsets_.size());
        return row_offsets_[node_idx + 1] - row_offsets_[node_idx];
    }

    const graph::NodeId* neighbors_begin(size_t node_idx) const {
        assert(node_idx + 1 < row_offsets_.size());
        return column_indices_.data() + row_offsets_[node_idx];
    }

    const graph::NodeId* neighbors_end(size_t node_idx) const {
        assert(node_idx + 1 < row_offsets_.size());
        return column_indices_.data() + row_offsets_[node_idx + 1];
    }

    size_t num_nodes() const noexcept { return row_offsets_.empty() ? 0 : row_offsets_.size() - 1; }
    size_t num_edges() const noexcept { return column_indices_.size() / 2; }

    const std::vector<size_t>& row_offsets() const noexcept { return row_offsets_; }
    const std::vector<graph::NodeId>& column_indices() const noexcept { return column_indices_; }

private:
    std::vector<size_t> row_offsets_;
    std::vector<graph::NodeId> column_indices_;
    std::vector<std::pair<graph::NodeId, const NodeT*>> node_ids_;
    bool valid_{false};
};

} // namespace satgraf
