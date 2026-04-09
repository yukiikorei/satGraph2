#include <catch2/catch_test_macros.hpp>

#include "satgraf/layout.hpp"

#include <algorithm>
#include <cmath>
#include <set>

using namespace satgraf::graph;
using namespace satgraf::layout;

namespace {

class DummyLayout : public Layout {
public:
    CoordinateMap compute(const Graph<Node, Edge>& graph,
                          ProgressCallback progress_callback = nullptr) override {
        CoordinateMap coordinates;

        for (const auto& [node_id, node] : graph.getNodes()) {
            (void)node;
            const auto node_value = static_cast<uint32_t>(node_id);
            coordinates.emplace(node_id,
                                Coordinate{static_cast<double>(node_value),
                                           static_cast<double>(node_value * 2U)});
        }

        if (progress_callback) {
            progress_callback(1.0);
        }

        return coordinates;
    }
};

class CustomLayout : public Layout {
public:
    CoordinateMap compute(const Graph<Node, Edge>&,
                          ProgressCallback = nullptr) override {
        return {};
    }
};

Graph<Node, Edge> make_small_graph() {
    Graph<Node, Edge> graph;

    graph.createNode(NodeId{0}, "a");
    graph.createNode(NodeId{1}, "b");
    graph.createNode(NodeId{2}, "c");

    auto& edge = graph.createEdge(NodeId{0}, NodeId{1});
    graph.connect(edge.id, NodeId{0});

    return graph;
}

Graph<Node, Edge> make_path_graph(std::size_t count) {
    Graph<Node, Edge> graph;

    for (std::size_t i = 0; i < count; ++i) {
        graph.createNode(NodeId{static_cast<uint32_t>(i)}, "n" + std::to_string(i));
    }

    for (std::size_t i = 0; i + 1 < count; ++i) {
        auto& edge = graph.createEdge(NodeId{static_cast<uint32_t>(i)},
                                      NodeId{static_cast<uint32_t>(i + 1)});
        graph.connect(edge.id, NodeId{static_cast<uint32_t>(i)});
    }

    return graph;
}

Graph<Node, Edge> make_cluster_graph() {
    Graph<Node, Edge> graph;

    for (uint32_t i = 0; i < 6; ++i) {
        graph.createNode(NodeId{i}, "n" + std::to_string(i));
    }

    const std::pair<uint32_t, uint32_t> edges[] = {
        {0, 1}, {1, 2}, {0, 2}, {3, 4}, {4, 5}, {3, 5}, {2, 3}
    };

    for (const auto& [source, target] : edges) {
        auto& edge = graph.createEdge(NodeId{source}, NodeId{target});
        graph.connect(edge.id, NodeId{source});
    }

    return graph;
}

Graph<Node, Edge> make_disconnected_pairs_graph() {
    Graph<Node, Edge> graph;

    for (uint32_t i = 0; i < 6; ++i) {
        graph.createNode(NodeId{i}, "d" + std::to_string(i));
    }

    const std::pair<uint32_t, uint32_t> edges[] = {
        {0, 1}, {1, 2}, {3, 4}, {4, 5}
    };

    for (const auto& [source, target] : edges) {
        auto& edge = graph.createEdge(NodeId{source}, NodeId{target});
        graph.connect(edge.id, NodeId{source});
    }

    return graph;
}

Graph<Node, Edge> make_weighted_graph() {
    Graph<Node, Edge> graph;
    for (uint32_t i = 0; i < 4; ++i) {
        graph.createNode(NodeId{i}, "w" + std::to_string(i));
    }

    auto& heavy = graph.createEdge(NodeId{0}, NodeId{1});
    heavy.weight = 10.0;
    graph.connect(heavy.id, NodeId{0});

    auto& light = graph.createEdge(NodeId{2}, NodeId{3});
    light.weight = 1.0;
    graph.connect(light.id, NodeId{2});

    auto& bridge = graph.createEdge(NodeId{1}, NodeId{2});
    bridge.weight = 0.5;
    graph.connect(bridge.id, NodeId{1});

    return graph;
}

double distance_between(const Coordinate& a, const Coordinate& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::unordered_map<NodeId, CommunityId> make_three_community_assignment() {
    return {
        {NodeId{0}, CommunityId{0}},
        {NodeId{1}, CommunityId{0}},
        {NodeId{2}, CommunityId{1}},
        {NodeId{3}, CommunityId{1}},
        {NodeId{4}, CommunityId{2}},
        {NodeId{5}, CommunityId{2}}
    };
}

void require_bounded_coordinates(const CoordinateMap& coordinates,
                                 const Graph<Node, Edge>& graph) {
    REQUIRE(coordinates.size() == graph.nodeCount());
    for (const auto& [node_id, node] : graph.getNodes()) {
        (void)node;
        const auto it = coordinates.find(node_id);
        REQUIRE(it != coordinates.end());
        REQUIRE(std::isfinite(it->second.x));
        REQUIRE(std::isfinite(it->second.y));
        REQUIRE(it->second.x >= 0.0);
        REQUIRE(it->second.x <= 1024.0);
        REQUIRE(it->second.y >= 0.0);
        REQUIRE(it->second.y <= 1024.0);
    }
}

}

TEST_CASE("Layout interface computes coordinates for each node", "[layout]") {
    DummyLayout layout;
    const auto graph = make_small_graph();

    const auto coordinates = layout.compute(graph);

    REQUIRE(coordinates.size() == graph.nodeCount());
    REQUIRE(coordinates.find(NodeId{0}) != coordinates.end());
    REQUIRE(coordinates.find(NodeId{1}) != coordinates.end());
    REQUIRE(coordinates.find(NodeId{2}) != coordinates.end());
    REQUIRE(coordinates.at(NodeId{1}).x == 1.0);
    REQUIRE(coordinates.at(NodeId{1}).y == 2.0);
}

TEST_CASE("Layout interface forwards progress callback", "[layout][progress]") {
    DummyLayout layout;
    const auto graph = make_small_graph();

    double reported_progress = 0.0;

    const auto coordinates = layout.compute(graph, [&](double progress) {
        reported_progress = progress;
    });

    REQUIRE(coordinates.size() == graph.nodeCount());
    REQUIRE(reported_progress == 1.0);
}

TEST_CASE("Factory creates layouts by short name", "[layout][factory]") {
    auto& factory = LayoutFactory::instance();

    SECTION("Create Fruchterman-Reingold") {
        auto layout = factory.create("f");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<FruchtermanReingoldLayout*>(layout.get()) != nullptr);
    }

    SECTION("Create ForceAtlas2") {
        auto layout = factory.create("forceAtlas2");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<ForceAtlas2Layout*>(layout.get()) != nullptr);
    }

    SECTION("Create Kamada-Kawai") {
        auto layout = factory.create("kk");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<KamadaKawaiLayout*>(layout.get()) != nullptr);
    }

    SECTION("Create circular") {
        auto layout = factory.create("c");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<CircularLayout*>(layout.get()) != nullptr);
    }

    SECTION("Create grid") {
        auto layout = factory.create("grid");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<GridLayout*>(layout.get()) != nullptr);
    }

    SECTION("Create grid+KK") {
        auto layout = factory.create("gkk");
        REQUIRE(layout != nullptr);
        REQUIRE(dynamic_cast<GridKamadaKawaiLayout*>(layout.get()) != nullptr);
    }

    SECTION("Unknown name throws") {
        REQUIRE_THROWS_AS(factory.create("unknown"), std::runtime_error);
    }
}

TEST_CASE("Factory lists required layout names", "[layout][factory]") {
    auto names = LayoutFactory::instance().available_algorithms();

    REQUIRE(names.size() >= 6);
    REQUIRE(std::find(names.begin(), names.end(), "f") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "forceAtlas2") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "kk") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "c") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "grid") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "gkk") != names.end());
}

TEST_CASE("Factory registers custom layout", "[layout][factory]") {
    auto& factory = LayoutFactory::instance();
    const std::string custom_name{"custom_layout"};

    factory.register_layout(custom_name, [] {
        return std::make_unique<CustomLayout>();
    });

    auto layout = factory.create(custom_name);
    REQUIRE(layout != nullptr);
    REQUIRE(dynamic_cast<CustomLayout*>(layout.get()) != nullptr);

    const auto names = factory.available_algorithms();
    REQUIRE(std::find(names.begin(), names.end(), custom_name) != names.end());
}

TEST_CASE("Fruchterman-Reingold produces bounded finite coordinates",
          "[layout][fr]") {
    FruchtermanReingoldLayout layout;
    const auto graph = make_path_graph(8);

    const auto coordinates = layout.compute(graph);

    REQUIRE(coordinates.size() == graph.nodeCount());

    std::set<std::pair<long long, long long>> unique_positions;
    for (const auto& [node_id, node] : graph.getNodes()) {
        (void)node;
        const auto it = coordinates.find(node_id);
        REQUIRE(it != coordinates.end());
        REQUIRE(std::isfinite(it->second.x));
        REQUIRE(std::isfinite(it->second.y));
        REQUIRE(it->second.x >= 0.0);
        REQUIRE(it->second.x <= 1024.0);
        REQUIRE(it->second.y >= 0.0);
        REQUIRE(it->second.y <= 1024.0);

        unique_positions.emplace(static_cast<long long>(std::llround(it->second.x * 1000.0)),
                                 static_cast<long long>(std::llround(it->second.y * 1000.0)));
    }

    REQUIRE(unique_positions.size() == graph.nodeCount());
}

TEST_CASE("Fruchterman-Reingold progress is monotonic and completes",
          "[layout][fr][progress]") {
    FruchtermanReingoldLayout layout(100);
    const auto graph = make_path_graph(6);

    std::vector<double> progress_values;

    const auto coordinates = layout.compute(graph, [&](double progress) {
        progress_values.push_back(progress);
    });

    REQUIRE(coordinates.size() == graph.nodeCount());
    REQUIRE_FALSE(progress_values.empty());
    REQUIRE(progress_values.front() == 0.0);
    REQUIRE(progress_values.back() == 1.0);
    for (double value : progress_values) {
        REQUIRE(value >= 0.0);
        REQUIRE(value <= 1.0);
    }
    for (std::size_t i = 1; i < progress_values.size(); ++i) {
        REQUIRE(progress_values[i] >= progress_values[i - 1]);
    }
}

TEST_CASE("Fruchterman-Reingold tends to keep connected nodes closer",
          "[layout][fr]") {
    FruchtermanReingoldLayout layout;
    const auto graph = make_cluster_graph();
    const auto coordinates = layout.compute(graph);

    const double connected_average =
        (distance_between(coordinates.at(NodeId{0}), coordinates.at(NodeId{1})) +
         distance_between(coordinates.at(NodeId{1}), coordinates.at(NodeId{2})) +
         distance_between(coordinates.at(NodeId{3}), coordinates.at(NodeId{4})) +
         distance_between(coordinates.at(NodeId{4}), coordinates.at(NodeId{5}))) /
        4.0;

    const double unconnected_average =
        (distance_between(coordinates.at(NodeId{0}), coordinates.at(NodeId{5})) +
         distance_between(coordinates.at(NodeId{1}), coordinates.at(NodeId{4})) +
         distance_between(coordinates.at(NodeId{0}), coordinates.at(NodeId{4})) +
         distance_between(coordinates.at(NodeId{1}), coordinates.at(NodeId{5}))) /
        4.0;

    REQUIRE(connected_average < unconnected_average);
}

TEST_CASE("Kamada-Kawai produces bounded finite coordinates", "[layout][kk]") {
    KamadaKawaiLayout layout;
    const auto graph = make_path_graph(8);

    const auto coordinates = layout.compute(graph);

    REQUIRE(coordinates.size() == graph.nodeCount());
    for (const auto& [node_id, node] : graph.getNodes()) {
        (void)node;
        const auto it = coordinates.find(node_id);
        REQUIRE(it != coordinates.end());
        REQUIRE(std::isfinite(it->second.x));
        REQUIRE(std::isfinite(it->second.y));
        REQUIRE(it->second.x >= 0.0);
        REQUIRE(it->second.x <= 1024.0);
        REQUIRE(it->second.y >= 0.0);
        REQUIRE(it->second.y <= 1024.0);
    }
}

TEST_CASE("Kamada-Kawai separates disconnected components", "[layout][kk]") {
    KamadaKawaiLayout layout;
    const auto graph = make_disconnected_pairs_graph();

    const auto coordinates = layout.compute(graph);

    const double left_center =
        (coordinates.at(NodeId{0}).x + coordinates.at(NodeId{1}).x + coordinates.at(NodeId{2}).x) /
        3.0;
    const double right_center =
        (coordinates.at(NodeId{3}).x + coordinates.at(NodeId{4}).x + coordinates.at(NodeId{5}).x) /
        3.0;

    REQUIRE(std::abs(left_center - right_center) > 100.0);
}

TEST_CASE("Kamada-Kawai progress is monotonic and completes", "[layout][kk][progress]") {
    KamadaKawaiLayout layout(80);
    const auto graph = make_path_graph(7);

    std::vector<double> progress_values;
    const auto coordinates = layout.compute(graph, [&](double progress) {
        progress_values.push_back(progress);
    });

    REQUIRE(coordinates.size() == graph.nodeCount());
    REQUIRE_FALSE(progress_values.empty());
    REQUIRE(progress_values.front() == 0.0);
    REQUIRE(progress_values.back() == 1.0);
    for (double value : progress_values) {
        REQUIRE(value >= 0.0);
        REQUIRE(value <= 1.0);
    }
    for (std::size_t i = 1; i < progress_values.size(); ++i) {
        REQUIRE(progress_values[i] >= progress_values[i - 1]);
    }
}

TEST_CASE("ForceAtlas2 produces bounded finite coordinates", "[layout][fa2]") {
    ForceAtlas2Layout layout;
    const auto graph = make_cluster_graph();

    const auto coordinates = layout.compute(graph);

    REQUIRE(coordinates.size() == graph.nodeCount());
    for (const auto& [node_id, node] : graph.getNodes()) {
        (void)node;
        const auto it = coordinates.find(node_id);
        REQUIRE(it != coordinates.end());
        REQUIRE(std::isfinite(it->second.x));
        REQUIRE(std::isfinite(it->second.y));
        REQUIRE(it->second.x >= 0.0);
        REQUIRE(it->second.x <= 1024.0);
        REQUIRE(it->second.y >= 0.0);
        REQUIRE(it->second.y <= 1024.0);
    }
}

TEST_CASE("ForceAtlas2 progress is monotonic and completes", "[layout][fa2][progress]") {
    ForceAtlas2Layout layout(80, 1024.0, 1024.0, 0.0, 10.0, 1.0, false, 1.0, true, 1.2, 2);
    const auto graph = make_path_graph(10);

    std::vector<double> progress_values;
    const auto coordinates = layout.compute(graph, [&](double progress) {
        progress_values.push_back(progress);
    });

    REQUIRE(coordinates.size() == graph.nodeCount());
    REQUIRE_FALSE(progress_values.empty());
    REQUIRE(progress_values.front() == 0.0);
    REQUIRE(progress_values.back() == 1.0);
    for (double value : progress_values) {
        REQUIRE(value >= 0.0);
        REQUIRE(value <= 1.0);
    }
    for (std::size_t i = 1; i < progress_values.size(); ++i) {
        REQUIRE(progress_values[i] >= progress_values[i - 1]);
    }
}

TEST_CASE("ForceAtlas2 edge weight influence pulls heavier edges closer", "[layout][fa2]") {
    ForceAtlas2Layout layout(120, 1024.0, 1024.0, 0.0, 10.0, 1.0, false, 1.0, true, 1.2, 2);
    const auto graph = make_weighted_graph();

    const auto coordinates = layout.compute(graph);

    const double heavy_distance = distance_between(coordinates.at(NodeId{0}), coordinates.at(NodeId{1}));
    const double light_distance = distance_between(coordinates.at(NodeId{2}), coordinates.at(NodeId{3}));

    REQUIRE(heavy_distance < light_distance);
}

TEST_CASE("Community-aware layouts produce bounded coordinates for every node", "[layout][community]") {
    const auto graph = make_cluster_graph();
    const auto assignment = make_three_community_assignment();

    SECTION("Circular layout") {
        CircularLayout layout;
        layout.setCommunityAssignment(assignment);
        require_bounded_coordinates(layout.compute(graph), graph);
    }

    SECTION("Grid layout") {
        GridLayout layout;
        layout.setCommunityAssignment(assignment);
        require_bounded_coordinates(layout.compute(graph), graph);
    }

    SECTION("Grid+KK layout") {
        GridKamadaKawaiLayout layout;
        layout.setCommunityAssignment(assignment);
        require_bounded_coordinates(layout.compute(graph), graph);
    }

    SECTION("Community ForceAtlas2 layout") {
        CommunityForceAtlas2Layout layout;
        layout.setCommunityAssignment(assignment);
        require_bounded_coordinates(layout.compute(graph), graph);
    }
}

TEST_CASE("Community-aware layouts report monotonic progress", "[layout][community][progress]") {
    const auto graph = make_cluster_graph();
    const auto assignment = make_three_community_assignment();

    SECTION("Circular progress") {
        CircularLayout layout;
        layout.setCommunityAssignment(assignment);
        std::vector<double> progress_values;
        require_bounded_coordinates(layout.compute(graph, [&](double progress) {
            progress_values.push_back(progress);
        }), graph);
        REQUIRE_FALSE(progress_values.empty());
        REQUIRE(progress_values.front() == 0.0);
        REQUIRE(progress_values.back() == 1.0);
        for (std::size_t i = 1; i < progress_values.size(); ++i) {
            REQUIRE(progress_values[i] >= progress_values[i - 1]);
        }
    }

    SECTION("Grid progress") {
        GridLayout layout;
        layout.setCommunityAssignment(assignment);
        std::vector<double> progress_values;
        require_bounded_coordinates(layout.compute(graph, [&](double progress) {
            progress_values.push_back(progress);
        }), graph);
        REQUIRE_FALSE(progress_values.empty());
        REQUIRE(progress_values.front() == 0.0);
        REQUIRE(progress_values.back() == 1.0);
        for (std::size_t i = 1; i < progress_values.size(); ++i) {
            REQUIRE(progress_values[i] >= progress_values[i - 1]);
        }
    }

    SECTION("Grid+KK progress") {
        GridKamadaKawaiLayout layout;
        layout.setCommunityAssignment(assignment);
        std::vector<double> progress_values;
        require_bounded_coordinates(layout.compute(graph, [&](double progress) {
            progress_values.push_back(progress);
        }), graph);
        REQUIRE_FALSE(progress_values.empty());
        REQUIRE(progress_values.front() == 0.0);
        REQUIRE(progress_values.back() == 1.0);
        for (std::size_t i = 1; i < progress_values.size(); ++i) {
            REQUIRE(progress_values[i] >= progress_values[i - 1]);
        }
    }

    SECTION("Community ForceAtlas2 progress") {
        CommunityForceAtlas2Layout layout;
        layout.setCommunityAssignment(assignment);
        std::vector<double> progress_values;
        require_bounded_coordinates(layout.compute(graph, [&](double progress) {
            progress_values.push_back(progress);
        }), graph);
        REQUIRE_FALSE(progress_values.empty());
        REQUIRE(progress_values.front() == 0.0);
        REQUIRE(progress_values.back() == 1.0);
        for (std::size_t i = 1; i < progress_values.size(); ++i) {
            REQUIRE(progress_values[i] >= progress_values[i - 1]);
        }
    }
}

TEST_CASE("All implemented layouts produce coordinates for every node", "[layout][all]") {
    const auto graph = make_cluster_graph();
    const auto assignment = make_three_community_assignment();

    require_bounded_coordinates(FruchtermanReingoldLayout{}.compute(graph), graph);
    require_bounded_coordinates(ForceAtlas2Layout{}.compute(graph), graph);
    require_bounded_coordinates(KamadaKawaiLayout{}.compute(graph), graph);

    CircularLayout circular;
    circular.setCommunityAssignment(assignment);
    require_bounded_coordinates(circular.compute(graph), graph);

    GridLayout grid;
    grid.setCommunityAssignment(assignment);
    require_bounded_coordinates(grid.compute(graph), graph);

    GridKamadaKawaiLayout gkk;
    gkk.setCommunityAssignment(assignment);
    require_bounded_coordinates(gkk.compute(graph), graph);

    CommunityForceAtlas2Layout cfa2;
    cfa2.setCommunityAssignment(assignment);
    require_bounded_coordinates(cfa2.compute(graph), graph);

    GpuFruchtermanReingoldLayout gpu_fr;
    require_bounded_coordinates(gpu_fr.compute(graph), graph);
}

TEST_CASE("GPU FR factory registration and type", "[layout][gpu]") {
    auto layout = LayoutFactory::instance().create("fgpu");
    REQUIRE(layout != nullptr);
    auto* gpu = dynamic_cast<GpuFruchtermanReingoldLayout*>(layout.get());
    REQUIRE(gpu != nullptr);
}

TEST_CASE("GPU FR produces bounded finite coordinates or falls back", "[layout][gpu]") {
    auto graph = make_cluster_graph();
    GpuFruchtermanReingoldLayout gpu_fr;
    auto coords = gpu_fr.compute(graph);
    require_bounded_coordinates(coords, graph);
}

TEST_CASE("GPU FR progress is monotonic and completes", "[layout][gpu]") {
    auto graph = make_cluster_graph();
    GpuFruchtermanReingoldLayout gpu_fr;
    std::vector<double> progress_values;
    auto coords = gpu_fr.compute(graph, [&](double progress) {
        progress_values.push_back(progress);
    });
    require_bounded_coordinates(coords, graph);
    REQUIRE_FALSE(progress_values.empty());
    REQUIRE(progress_values.front() == 0.0);
    REQUIRE(progress_values.back() == 1.0);
    for (std::size_t i = 1; i < progress_values.size(); ++i) {
        REQUIRE(progress_values[i] >= progress_values[i - 1]);
    }
}

TEST_CASE("fgpu is in available algorithms", "[layout][factory]") {
    const auto names = LayoutFactory::instance().available_algorithms();
    REQUIRE(std::find(names.begin(), names.end(), "fgpu") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "f") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "forceAtlas2") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "kk") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "c") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "grid") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "gkk") != names.end());
}
