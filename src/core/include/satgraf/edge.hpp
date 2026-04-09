#pragma once

#include "satgraf/types.hpp"

namespace satgraf::graph {

enum class EdgeVisibility { Shown, Hidden };
enum class EdgeType { Normal, Conflict };

struct Edge {
    EdgeId id;
    NodeId source;
    NodeId target;
    bool bidirectional{true};
    double weight{1.0};
    EdgeVisibility visibility{EdgeVisibility::Shown};
    EdgeType type{EdgeType::Normal};
    int degrees{0};

    Edge() = default;
    Edge(EdgeId eid, NodeId src, NodeId tgt)
        : id(eid), source(src), target(tgt) {}
};

} // namespace satgraf::graph
