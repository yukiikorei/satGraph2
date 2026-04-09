#pragma once

#include "satgraf/types.hpp"

#include <unordered_map>
#include <utility>
#include <vector>

namespace satgraf::graph {

class Clause {
public:
    using LiteralMap = std::unordered_map<NodeId, bool>;
    using const_iterator = LiteralMap::const_iterator;

    Clause() = default;

    void add_literal(NodeId node, bool polarity) {
        literals_[node] = polarity;
    }

    void remove_literal(NodeId node) {
        literals_.erase(node);
    }

    bool has_literal(NodeId node) const {
        return literals_.count(node) > 0;
    }

    bool get_polarity(NodeId node) const {
        return literals_.at(node);
    }

    bool empty() const noexcept {
        return literals_.empty();
    }

    size_t size() const noexcept {
        return literals_.size();
    }

    const_iterator begin() const noexcept { return literals_.begin(); }
    const_iterator end() const noexcept { return literals_.end(); }

    const LiteralMap& literals() const noexcept { return literals_; }

private:
    LiteralMap literals_;
};

} // namespace satgraf::graph
