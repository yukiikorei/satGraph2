#pragma once

#include "satgraf/node.hpp"

namespace satgraf::graph {

struct CommunityNode : Node {
    CommunityId community_id{invalid_community_id};
    bool bridge{false};

    CommunityNode() = default;
    CommunityNode(NodeId nid, std::string name)
        : Node(nid, std::move(name)) {}
};

} // namespace satgraf::graph
