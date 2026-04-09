#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "satgraf/community_detector.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_set>

using namespace satgraf::graph;
using namespace satgraf::community;

namespace {

class DummyDetector : public Detector {
public:
    CommunityResult detect(
        const Graph<Node, Edge>&) override {
        return CommunityResult{};
    }
    std::string name() const override { return "dummy"; }
};

Graph<Node, Edge> make_three_community_graph() {
    Graph<Node, Edge> g;

    g.createNode(NodeId{0}, "a0");
    g.createNode(NodeId{1}, "a1");
    g.createNode(NodeId{2}, "a2");
    g.createNode(NodeId{3}, "a3");

    auto& e01 = g.createEdge(NodeId{0}, NodeId{1});
    g.connect(e01.id, NodeId{0});
    auto& e02 = g.createEdge(NodeId{0}, NodeId{2});
    g.connect(e02.id, NodeId{0});
    auto& e03 = g.createEdge(NodeId{0}, NodeId{3});
    g.connect(e03.id, NodeId{0});
    auto& e12 = g.createEdge(NodeId{1}, NodeId{2});
    g.connect(e12.id, NodeId{1});
    auto& e13 = g.createEdge(NodeId{1}, NodeId{3});
    g.connect(e13.id, NodeId{1});
    auto& e23 = g.createEdge(NodeId{2}, NodeId{3});
    g.connect(e23.id, NodeId{2});

    g.createNode(NodeId{4}, "b0");
    g.createNode(NodeId{5}, "b1");
    g.createNode(NodeId{6}, "b2");

    auto& e45 = g.createEdge(NodeId{4}, NodeId{5});
    g.connect(e45.id, NodeId{4});
    auto& e46 = g.createEdge(NodeId{4}, NodeId{6});
    g.connect(e46.id, NodeId{4});
    auto& e56 = g.createEdge(NodeId{5}, NodeId{6});
    g.connect(e56.id, NodeId{5});

    g.createNode(NodeId{7}, "c0");
    g.createNode(NodeId{8}, "c1");
    g.createNode(NodeId{9}, "c2");

    auto& e78 = g.createEdge(NodeId{7}, NodeId{8});
    g.connect(e78.id, NodeId{7});
    auto& e79 = g.createEdge(NodeId{7}, NodeId{9});
    g.connect(e79.id, NodeId{7});
    auto& e89 = g.createEdge(NodeId{8}, NodeId{9});
    g.connect(e89.id, NodeId{8});

    auto& e_bridge_ab = g.createEdge(NodeId{1}, NodeId{4});
    g.connect(e_bridge_ab.id, NodeId{1});
    auto& e_bridge_bc = g.createEdge(NodeId{5}, NodeId{7});
    g.connect(e_bridge_bc.id, NodeId{5});

    return g;
}

bool same_community(const CommunityResult& r, NodeId a, NodeId b) {
    auto it_a = r.assignment.find(a);
    auto it_b = r.assignment.find(b);
    REQUIRE(it_a != r.assignment.end());
    REQUIRE(it_b != r.assignment.end());
    return it_a->second == it_b->second;
}

size_t count_communities(const CommunityResult& r) {
    std::unordered_set<uint32_t> cids;
    for (const auto& [nid, cid] : r.assignment) {
        (void)nid;
        cids.insert(static_cast<uint32_t>(cid));
    }
    return cids.size();
}

Graph<Node, Edge> make_disconnected_graph() {
    Graph<Node, Edge> g;

    g.createNode(NodeId{0}, "a");
    g.createNode(NodeId{1}, "b");
    auto& e01 = g.createEdge(NodeId{0}, NodeId{1});
    g.connect(e01.id, NodeId{0});

    g.createNode(NodeId{2}, "c");
    g.createNode(NodeId{3}, "d");
    auto& e23 = g.createEdge(NodeId{2}, NodeId{3});
    g.connect(e23.id, NodeId{2});

    return g;
}

}  // namespace

TEST_CASE("Louvain detects three communities", "[community][louvain]") {
    auto g = make_three_community_graph();
    LouvainDetector detector;
    auto result = detector.detect(g);

    REQUIRE(result.assignment.size() == 10);
    REQUIRE(result.q_modularity > 0.0);
    REQUIRE(count_communities(result) >= 2);

    REQUIRE(same_community(result, NodeId{0}, NodeId{1}));
    REQUIRE(same_community(result, NodeId{0}, NodeId{2}));
    REQUIRE(same_community(result, NodeId{0}, NodeId{3}));
    REQUIRE(same_community(result, NodeId{4}, NodeId{5}));
    REQUIRE(same_community(result, NodeId{4}, NodeId{6}));
    REQUIRE(same_community(result, NodeId{7}, NodeId{8}));
    REQUIRE(same_community(result, NodeId{7}, NodeId{9}));
}

TEST_CASE("CNM detects communities", "[community][cnm]") {
    auto g = make_three_community_graph();
    CNMDetector detector;
    auto result = detector.detect(g);

    REQUIRE(result.assignment.size() == 10);
    REQUIRE(result.q_modularity > 0.0);
    REQUIRE(count_communities(result) >= 2);

    REQUIRE(same_community(result, NodeId{0}, NodeId{1}));
    REQUIRE(same_community(result, NodeId{0}, NodeId{2}));
    REQUIRE(same_community(result, NodeId{0}, NodeId{3}));
    REQUIRE(same_community(result, NodeId{4}, NodeId{5}));
    REQUIRE(same_community(result, NodeId{4}, NodeId{6}));
    REQUIRE(same_community(result, NodeId{7}, NodeId{8}));
    REQUIRE(same_community(result, NodeId{7}, NodeId{9}));
}

TEST_CASE("Online detector produces valid communities", "[community][online]") {
    auto g = make_three_community_graph();
    OnlineDetector detector;
    auto result = detector.detect(g);

    REQUIRE(result.assignment.size() == 10);
    REQUIRE(count_communities(result) >= 1);

    REQUIRE(same_community(result, NodeId{0}, NodeId{1}));
    REQUIRE(same_community(result, NodeId{0}, NodeId{2}));

    REQUIRE(result.q_modularity >= 0.0);
}

TEST_CASE("Detector name returns correct identifier", "[community]") {
    REQUIRE(LouvainDetector().name() == "louvain");
    REQUIRE(CNMDetector().name() == "cnm");
    REQUIRE(OnlineDetector().name() == "online");
}

TEST_CASE("Factory creates detectors by name", "[community][factory]") {
    auto& factory = DetectorFactory::instance();

    SECTION("Create Louvain") {
        auto det = factory.create("louvain");
        REQUIRE(det != nullptr);
        REQUIRE(det->name() == "louvain");
    }

    SECTION("Create CNM") {
        auto det = factory.create("cnm");
        REQUIRE(det != nullptr);
        REQUIRE(det->name() == "cnm");
    }

    SECTION("Create Online") {
        auto det = factory.create("online");
        REQUIRE(det != nullptr);
        REQUIRE(det->name() == "online");
    }

    SECTION("Unknown name throws") {
        REQUIRE_THROWS_AS(factory.create("unknown"), std::runtime_error);
    }

    SECTION("Available algorithms lists all three") {
        auto names = factory.available_algorithms();
        REQUIRE(names.size() == 3);
        REQUIRE(std::find(names.begin(), names.end(), "louvain") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "cnm") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "online") != names.end());
    }
}

TEST_CASE("Factory register custom detector", "[community][factory]") {
    auto& factory = DetectorFactory::instance();

    factory.register_detector("dummy", [] {
        return std::make_unique<DummyDetector>();
    });

    auto det = factory.create("dummy");
    REQUIRE(det != nullptr);
    REQUIRE(det->name() == "dummy");

    auto names = factory.available_algorithms();
    REQUIRE(std::find(names.begin(), names.end(), "dummy") != names.end());
}

TEST_CASE("Disjoint graph triggers Louvain fallback to CNM",
          "[community][disjoint]") {
    auto g = make_disconnected_graph();

    LouvainDetector detector(true);
    auto result = detector.detect(g);

    REQUIRE(result.assignment.size() == 4);
    REQUIRE(count_communities(result) >= 2);
}

TEST_CASE("Disjoint graph Louvain without fallback still runs",
          "[community][disjoint]") {
    auto g = make_disconnected_graph();

    LouvainDetector detector(false);
    auto result = detector.detect(g);

    REQUIRE(result.assignment.size() == 4);
}

TEST_CASE("Community statistics computation", "[community][stats]") {
    auto g = make_three_community_graph();
    CNMDetector detector;
    auto result = detector.detect(g);

    REQUIRE(result.min_community_size > 0);
    REQUIRE(result.max_community_size >= result.min_community_size);
    REQUIRE(result.mean_community_size > 0.0);
    REQUIRE(result.sd_community_size >= 0.0);

    const size_t total =
        static_cast<size_t>(result.mean_community_size * count_communities(result) + 0.5);
    REQUIRE(total == result.assignment.size());
}

TEST_CASE("Edge classification inter/intra", "[community][edges]") {
    auto g = make_three_community_graph();
    CNMDetector detector;
    auto result = detector.detect(g);

    REQUIRE(result.intra_edges > 0);
    REQUIRE(result.inter_edges > 0);
    REQUIRE(result.intra_edges + result.inter_edges == g.edgeCount());
    REQUIRE(result.edge_ratio > 0.0);
}

TEST_CASE("Empty graph returns empty result", "[community][empty]") {
    Graph<Node, Edge> g;

    LouvainDetector louvain;
    auto lr = louvain.detect(g);
    REQUIRE(lr.assignment.empty());
    REQUIRE(lr.q_modularity == 0.0);

    CNMDetector cnm;
    auto cr = cnm.detect(g);
    REQUIRE(cr.assignment.empty());

    OnlineDetector online;
    auto orr = online.detect(g);
    REQUIRE(orr.assignment.empty());
}

TEST_CASE("Single node graph", "[community][single]") {
    Graph<Node, Edge> g;
    g.createNode(NodeId{0}, "solo");

    OnlineDetector detector;
    auto result = detector.detect(g);
    REQUIRE(result.assignment.size() == 1);
    REQUIRE(result.assignment.count(NodeId{0}) == 1);
}

TEST_CASE("Two node graph", "[community][small]") {
    Graph<Node, Edge> g;
    g.createNode(NodeId{0}, "a");
    g.createNode(NodeId{1}, "b");
    auto& e = g.createEdge(NodeId{0}, NodeId{1});
    g.connect(e.id, NodeId{0});

    LouvainDetector louvain;
    auto lr = louvain.detect(g);
    REQUIRE(lr.assignment.size() == 2);
    REQUIRE(same_community(lr, NodeId{0}, NodeId{1}));

    CNMDetector cnm;
    auto cr = cnm.detect(g);
    REQUIRE(cr.assignment.size() == 2);
    REQUIRE(same_community(cr, NodeId{0}, NodeId{1}));
}

TEST_CASE("Modularity is positive for clear community structure",
          "[community][modularity]") {
    auto g = make_three_community_graph();

    LouvainDetector louvain;
    auto lr = louvain.detect(g);
    REQUIRE(lr.q_modularity > 0.3);

    CNMDetector cnm;
    auto cr = cnm.detect(g);
    REQUIRE(cr.q_modularity > 0.3);
}

TEST_CASE("Connectivity check works", "[community][connectivity]") {
    SECTION("Connected graph") {
        auto g = make_three_community_graph();
        REQUIRE(detail::is_graph_connected(g));
    }

    SECTION("Disconnected graph") {
        auto g = make_disconnected_graph();
        REQUIRE_FALSE(detail::is_graph_connected(g));
    }

    SECTION("Single node is connected") {
        Graph<Node, Edge> g;
        g.createNode(NodeId{0}, "solo");
        REQUIRE(detail::is_graph_connected(g));
    }

    SECTION("Empty graph is connected") {
        Graph<Node, Edge> g;
        REQUIRE(detail::is_graph_connected(g));
    }
}
