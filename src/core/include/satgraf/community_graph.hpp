#pragma once

#include "satgraf/community_node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/graph.hpp"
#include "satgraf/types.hpp"

#include <cmath>
#include <map>
#include <optional>

namespace satgraf::graph {

class CommunityGraph : public Graph<CommunityNode, Edge> {
public:
    CommunityGraph() = default;

    double modularity() const noexcept { return modularity_; }
    void set_modularity(double q) { modularity_ = q; }

    const std::map<CommunityId, size_t>& communitySizes() const noexcept {
        return community_sizes_;
    }

    void rebuild_community_stats() {
        community_sizes_.clear();
        inter_community_edges_ = 0;
        intra_community_edges_ = 0;

        for (const auto& [nid, node] : getNodes()) {
            community_sizes_[node.community_id]++;
        }

        for (const auto& [eid, edge] : getEdges()) {
            auto src_it = getNodes().find(edge.source);
            auto tgt_it = getNodes().find(edge.target);
            if (src_it == getNodes().end() || tgt_it == getNodes().end()) continue;

            if (src_it->second.community_id == tgt_it->second.community_id) {
                intra_community_edges_++;
            } else {
                inter_community_edges_++;
            }
        }
    }

    size_t inter_community_edge_count() const noexcept {
        return inter_community_edges_;
    }

    size_t intra_community_edge_count() const noexcept {
        return intra_community_edges_;
    }

    double edge_ratio() const {
        if (intra_community_edges_ == 0) return 0.0;
        return static_cast<double>(inter_community_edges_) /
               static_cast<double>(intra_community_edges_);
    }

    struct CommunityStats {
        size_t min_size;
        size_t max_size;
        double mean_size;
        double sd_size;
    };

    std::optional<CommunityStats> compute_community_stats() const {
        if (community_sizes_.empty()) return std::nullopt;

        size_t min_s = community_sizes_.begin()->second;
        size_t max_s = 0;
        double sum = 0.0;

        for (const auto& [cid, sz] : community_sizes_) {
            min_s = std::min(min_s, sz);
            max_s = std::max(max_s, sz);
            sum += static_cast<double>(sz);
        }

        const double mean = sum / static_cast<double>(community_sizes_.size());

        double variance = 0.0;
        for (const auto& [cid, sz] : community_sizes_) {
            double diff = static_cast<double>(sz) - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(community_sizes_.size());

        return CommunityStats{min_s, max_s, mean, std::sqrt(variance)};
    }

private:
    double modularity_{0.0};
    std::map<CommunityId, size_t> community_sizes_;
    size_t inter_community_edges_{0};
    size_t intra_community_edges_{0};
};

} // namespace satgraf::graph
