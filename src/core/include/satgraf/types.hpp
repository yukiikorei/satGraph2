#pragma once

#include <cstdint>
#include <functional>
#include <limits>

namespace satgraf::graph {

template<typename Tag>
struct StrongId {
    uint32_t value;

    constexpr StrongId() noexcept : value(std::numeric_limits<uint32_t>::max()) {}
    constexpr explicit StrongId(uint32_t v) noexcept : value(v) {}

    constexpr bool operator==(const StrongId& other) const noexcept {
        return value == other.value;
    }
    constexpr bool operator!=(const StrongId& other) const noexcept {
        return value != other.value;
    }
    constexpr bool operator<(const StrongId& other) const noexcept {
        return value < other.value;
    }
    constexpr bool operator<=(const StrongId& other) const noexcept {
        return value <= other.value;
    }
    constexpr bool operator>(const StrongId& other) const noexcept {
        return value > other.value;
    }
    constexpr bool operator>=(const StrongId& other) const noexcept {
        return value >= other.value;
    }

    constexpr explicit operator uint32_t() const noexcept { return value; }
};

struct NodeIdTag {};
struct EdgeIdTag {};
struct CommunityIdTag {};

using NodeId = StrongId<NodeIdTag>;
using EdgeId = StrongId<EdgeIdTag>;
using CommunityId = StrongId<CommunityIdTag>;

constexpr NodeId invalid_node_id{};
constexpr EdgeId invalid_edge_id{};
constexpr CommunityId invalid_community_id{};

} // namespace satgraf::graph

template<typename Tag>
struct std::hash<satgraf::graph::StrongId<Tag>> {
    size_t operator()(const satgraf::graph::StrongId<Tag>& id) const noexcept {
        return std::hash<uint32_t>{}(id.value);
    }
};
