#pragma once

#include "satgraf/types.hpp"

#include <string>
#include <vector>

namespace satgraf::graph {

enum class Assignment { True, False, Unassigned };

struct Node {
    NodeId id;
    std::string name;
    std::vector<EdgeId> edges;
    std::vector<std::string> groups;
    Assignment assignment{Assignment::Unassigned};
    double activity{0.0};
    std::vector<int> appearance_counts;

    Node() = default;
    Node(NodeId nid, std::string nname)
        : id(nid), name(std::move(nname)) {}
};

} // namespace satgraf::graph
