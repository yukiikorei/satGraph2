#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "satgraf/types.hpp"
#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/clause.hpp"
#include "satgraf/graph.hpp"
#include "satgraf/community_node.hpp"
#include "satgraf/community_graph.hpp"
#include "satgraf/csr.hpp"
#include "satgraf/union_find.hpp"
#include "satgraf/dimacs_writer.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace satgraf::graph;
using namespace satgraf;

TEST_CASE("Strong typedefs comparison and hashing", "[types]") {
    SECTION("NodeId equality and inequality") {
        NodeId a{1};
        NodeId b{1};
        NodeId c{2};
        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
        REQUIRE(a != c);
    }

    SECTION("NodeId ordering") {
        NodeId a{1};
        NodeId b{2};
        REQUIRE(a < b);
        REQUIRE(b > a);
        REQUIRE(a <= b);
        REQUIRE(b >= a);
        REQUIRE(a <= NodeId{1});
        REQUIRE(a >= NodeId{1});
    }

    SECTION("EdgeId and CommunityId independent types") {
        EdgeId e{1};
        NodeId n{1};
        CommunityId c{1};
        REQUIRE(static_cast<uint32_t>(e) == 1u);
        REQUIRE(static_cast<uint32_t>(n) == 1u);
        REQUIRE(static_cast<uint32_t>(c) == 1u);
    }

    SECTION("Hashing works with unordered containers") {
        std::unordered_set<NodeId> node_set;
        node_set.insert(NodeId{1});
        node_set.insert(NodeId{2});
        node_set.insert(NodeId{1});
        REQUIRE(node_set.size() == 2);
        REQUIRE(node_set.count(NodeId{1}) == 1);
        REQUIRE(node_set.count(NodeId{3}) == 0);
    }

    SECTION("Hashing for EdgeId") {
        std::unordered_map<EdgeId, std::string> edge_map;
        edge_map[EdgeId{10}] = "edge10";
        REQUIRE(edge_map[EdgeId{10}] == "edge10");
        REQUIRE(edge_map.count(EdgeId{99}) == 0);
    }

    SECTION("Invalid sentinel values") {
        REQUIRE(invalid_node_id == NodeId{});
        REQUIRE(invalid_edge_id == EdgeId{});
        REQUIRE(invalid_community_id == CommunityId{});
    }
}

TEST_CASE("Node creation and defaults", "[node]") {
    SECTION("Default constructed node") {
        Node n;
        REQUIRE(n.assignment == Assignment::Unassigned);
        REQUIRE(n.activity == 0.0);
        REQUIRE(n.edges.empty());
        REQUIRE(n.groups.empty());
        REQUIRE(n.appearance_counts.empty());
    }

    SECTION("Node with id and name") {
        Node n{NodeId{42}, "var_x"};
        REQUIRE(static_cast<uint32_t>(n.id) == 42u);
        REQUIRE(n.name == "var_x");
        REQUIRE(n.assignment == Assignment::Unassigned);
    }

    SECTION("Assignment state changes") {
        Node n;
        n.assignment = Assignment::True;
        REQUIRE(n.assignment == Assignment::True);
        n.assignment = Assignment::False;
        REQUIRE(n.assignment == Assignment::False);
        n.assignment = Assignment::Unassigned;
        REQUIRE(n.assignment == Assignment::Unassigned);
    }

    SECTION("Activity counter") {
        Node n;
        n.activity = 1.5;
        REQUIRE(n.activity == 1.5);
        n.activity += 0.5;
        REQUIRE(n.activity == 2.0);
    }

    SECTION("Groups and appearance counts") {
        Node n;
        n.groups.push_back("group_a");
        n.groups.push_back("group_b");
        REQUIRE(n.groups.size() == 2);
        n.appearance_counts.push_back(3);
        n.appearance_counts.push_back(7);
        REQUIRE(n.appearance_counts.size() == 2);
    }
}

TEST_CASE("Edge creation and properties", "[edge]") {
    SECTION("Default constructed edge") {
        Edge e;
        REQUIRE(e.bidirectional == true);
        REQUIRE(e.weight == 1.0);
        REQUIRE(e.visibility == EdgeVisibility::Shown);
        REQUIRE(e.type == EdgeType::Normal);
        REQUIRE(e.degrees == 0);
    }

    SECTION("Edge with source and target") {
        Edge e{EdgeId{5}, NodeId{1}, NodeId{2}};
        REQUIRE(static_cast<uint32_t>(e.id) == 5u);
        REQUIRE(e.source == NodeId{1});
        REQUIRE(e.target == NodeId{2});
    }

    SECTION("Weight modification") {
        Edge e;
        e.weight = 3.7;
        REQUIRE(e.weight == 3.7);
    }

    SECTION("Visibility toggle") {
        Edge e;
        e.visibility = EdgeVisibility::Hidden;
        REQUIRE(e.visibility == EdgeVisibility::Hidden);
        e.visibility = EdgeVisibility::Shown;
        REQUIRE(e.visibility == EdgeVisibility::Shown);
    }

    SECTION("Edge type") {
        Edge e;
        e.type = EdgeType::Conflict;
        REQUIRE(e.type == EdgeType::Conflict);
    }
}

TEST_CASE("Clause operations", "[clause]") {
    SECTION("Empty clause") {
        Clause c;
        REQUIRE(c.empty());
        REQUIRE(c.size() == 0);
    }

    SECTION("Add and check literals") {
        Clause c;
        c.add_literal(NodeId{1}, true);
        c.add_literal(NodeId{2}, false);
        c.add_literal(NodeId{3}, true);
        REQUIRE(c.size() == 3);
        REQUIRE(c.has_literal(NodeId{1}));
        REQUIRE(c.get_polarity(NodeId{1}) == true);
        REQUIRE(c.has_literal(NodeId{2}));
        REQUIRE(c.get_polarity(NodeId{2}) == false);
        REQUIRE_FALSE(c.has_literal(NodeId{99}));
    }

    SECTION("Remove literal") {
        Clause c;
        c.add_literal(NodeId{1}, true);
        c.add_literal(NodeId{2}, false);
        c.remove_literal(NodeId{1});
        REQUIRE(c.size() == 1);
        REQUIRE_FALSE(c.has_literal(NodeId{1}));
        REQUIRE(c.has_literal(NodeId{2}));
    }

    SECTION("Overwrite literal polarity") {
        Clause c;
        c.add_literal(NodeId{1}, true);
        c.add_literal(NodeId{1}, false);
        REQUIRE(c.size() == 1);
        REQUIRE(c.get_polarity(NodeId{1}) == false);
    }

    SECTION("Iteration") {
        Clause c;
        c.add_literal(NodeId{1}, true);
        c.add_literal(NodeId{2}, false);
        size_t count = 0;
        for (const auto& [nid, polarity] : c) {
            (void)nid;
            (void)polarity;
            count++;
        }
        REQUIRE(count == 2);
    }
}

TEST_CASE("Graph CRUD operations", "[graph]") {
    Graph<Node, Edge> g;

    SECTION("Create and get nodes") {
        auto& n1 = g.createNode(NodeId{1}, "a");
        auto& n2 = g.createNode(NodeId{2}, "b");
        REQUIRE(static_cast<uint32_t>(n1.id) == 1u);
        REQUIRE(n1.name == "a");
        REQUIRE(g.nodeCount() == 2);

        auto opt = g.getNode(NodeId{1});
        REQUIRE(opt.has_value());
        REQUIRE(opt->get().name == "a");

        auto missing = g.getNode(NodeId{99});
        REQUIRE_FALSE(missing.has_value());
    }

    SECTION("Create edges") {
        g.createNode(NodeId{1}, "a");
        g.createNode(NodeId{2}, "b");
        auto& e = g.createEdge(NodeId{1}, NodeId{2});
        REQUIRE(e.source == NodeId{1});
        REQUIRE(e.target == NodeId{2});
        REQUIRE(g.edgeCount() == 1);
    }

    SECTION("Connect edge to nodes") {
        g.createNode(NodeId{1}, "a");
        g.createNode(NodeId{2}, "b");
        auto& e = g.createEdge(NodeId{1}, NodeId{2});
        g.connect(e.id, NodeId{1});

        auto& n1 = g.getNode(NodeId{1})->get();
        auto& n2 = g.getNode(NodeId{2})->get();
        REQUIRE(n1.edges.size() == 1);
        REQUIRE(n2.edges.size() == 1);
    }

    SECTION("Remove node cascades to edges") {
        g.createNode(NodeId{1}, "a");
        g.createNode(NodeId{2}, "b");
        g.createNode(NodeId{3}, "c");
        auto& e12 = g.createEdge(NodeId{1}, NodeId{2});
        g.connect(e12.id, NodeId{1});
        auto& e13 = g.createEdge(NodeId{1}, NodeId{3});
        g.connect(e13.id, NodeId{1});

        REQUIRE(g.nodeCount() == 3);
        REQUIRE(g.edgeCount() == 2);

        g.removeNode(NodeId{1});
        REQUIRE(g.nodeCount() == 2);
        REQUIRE(g.edgeCount() == 0);

        auto& n2 = g.getNode(NodeId{2})->get();
        auto& n3 = g.getNode(NodeId{3})->get();
        REQUIRE(n2.edges.empty());
        REQUIRE(n3.edges.empty());
    }

    SECTION("Remove edge cleans up adjacency lists") {
        g.createNode(NodeId{1}, "a");
        g.createNode(NodeId{2}, "b");
        auto& e = g.createEdge(NodeId{1}, NodeId{2});
        g.connect(e.id, NodeId{1});

        REQUIRE(g.getNode(NodeId{1})->get().edges.size() == 1);
        REQUIRE(g.getNode(NodeId{2})->get().edges.size() == 1);

        g.removeEdge(e.id);
        REQUIRE(g.edgeCount() == 0);
        REQUIRE(g.getNode(NodeId{1})->get().edges.empty());
        REQUIRE(g.getNode(NodeId{2})->get().edges.empty());
    }

    SECTION("Get nodes and edges ranges") {
        g.createNode(NodeId{1}, "a");
        g.createNode(NodeId{2}, "b");
        g.createEdge(NodeId{1}, NodeId{2});

        REQUIRE(g.getNodes().size() == 2);
        REQUIRE(g.getEdges().size() == 1);
    }

    SECTION("Clauses") {
        auto& c = g.addClause();
        c.add_literal(NodeId{1}, true);
        c.add_literal(NodeId{2}, false);
        REQUIRE(g.getClauses().size() == 1);
        REQUIRE(g.getClauses()[0].size() == 2);
    }

    SECTION("Get const node") {
        g.createNode(NodeId{1}, "a");
        const auto& cg = g;
        auto opt = cg.getNode(NodeId{1});
        REQUIRE(opt.has_value());
        REQUIRE(opt->get().name == "a");
    }
}

TEST_CASE("CommunityNode", "[community_node]") {
    SECTION("Default community is invalid") {
        CommunityNode cn;
        REQUIRE(cn.community_id == invalid_community_id);
        REQUIRE(cn.bridge == false);
    }

    SECTION("Community assignment") {
        CommunityNode cn{NodeId{5}, "node5"};
        cn.community_id = CommunityId{3};
        REQUIRE(cn.community_id == CommunityId{3});
    }

    SECTION("Bridge flag") {
        CommunityNode cn;
        cn.bridge = true;
        REQUIRE(cn.bridge == true);
    }

    SECTION("Inherits Node fields") {
        CommunityNode cn{NodeId{10}, "var10"};
        REQUIRE(static_cast<uint32_t>(cn.id) == 10u);
        REQUIRE(cn.name == "var10");
        REQUIRE(cn.assignment == Assignment::Unassigned);
        cn.assignment = Assignment::True;
        REQUIRE(cn.assignment == Assignment::True);
    }
}

TEST_CASE("CommunityGraph statistics", "[community_graph]") {
    CommunityGraph cg;

    SECTION("Empty graph stats") {
        REQUIRE(cg.modularity() == 0.0);
        auto stats = cg.compute_community_stats();
        REQUIRE_FALSE(stats.has_value());
    }

    SECTION("Community stats computation") {
        auto& n1 = cg.createNode(NodeId{1}, "a");
        n1.community_id = CommunityId{0};
        auto& n2 = cg.createNode(NodeId{2}, "b");
        n2.community_id = CommunityId{0};
        auto& n3 = cg.createNode(NodeId{3}, "c");
        n3.community_id = CommunityId{1};

        auto& e12 = cg.createEdge(NodeId{1}, NodeId{2});
        cg.connect(e12.id, NodeId{1});
        auto& e13 = cg.createEdge(NodeId{1}, NodeId{3});
        cg.connect(e13.id, NodeId{1});

        cg.rebuild_community_stats();

        auto sizes = cg.communitySizes();
        REQUIRE(sizes.at(CommunityId{0}) == 2);
        REQUIRE(sizes.at(CommunityId{1}) == 1);

        REQUIRE(cg.intra_community_edge_count() == 1);
        REQUIRE(cg.inter_community_edge_count() == 1);

        auto stats = cg.compute_community_stats();
        REQUIRE(stats.has_value());
        REQUIRE(stats->min_size == 1);
        REQUIRE(stats->max_size == 2);
        REQUIRE(stats->mean_size == 1.5);
    }

    SECTION("Edge ratio") {
        auto& n1 = cg.createNode(NodeId{1}, "a");
        n1.community_id = CommunityId{0};
        auto& n2 = cg.createNode(NodeId{2}, "b");
        n2.community_id = CommunityId{0};
        auto& n3 = cg.createNode(NodeId{3}, "c");
        n3.community_id = CommunityId{1};
        auto& n4 = cg.createNode(NodeId{4}, "d");
        n4.community_id = CommunityId{1};

        auto& e12 = cg.createEdge(NodeId{1}, NodeId{2});
        cg.connect(e12.id, NodeId{1});
        auto& e34 = cg.createEdge(NodeId{3}, NodeId{4});
        cg.connect(e34.id, NodeId{3});
        auto& e13 = cg.createEdge(NodeId{1}, NodeId{3});
        cg.connect(e13.id, NodeId{1});

        cg.rebuild_community_stats();

        REQUIRE(cg.intra_community_edge_count() == 2);
        REQUIRE(cg.inter_community_edge_count() == 1);
        REQUIRE(cg.edge_ratio() == 0.5);
    }

    SECTION("SD computation") {
        auto& n1 = cg.createNode(NodeId{1}, "a");
        n1.community_id = CommunityId{0};
        auto& n2 = cg.createNode(NodeId{2}, "b");
        n2.community_id = CommunityId{1};
        auto& n3 = cg.createNode(NodeId{3}, "c");
        n3.community_id = CommunityId{2};
        auto& n4 = cg.createNode(NodeId{4}, "d");
        n4.community_id = CommunityId{2};
        auto& n5 = cg.createNode(NodeId{5}, "e");
        n5.community_id = CommunityId{2};

        cg.rebuild_community_stats();

        auto stats = cg.compute_community_stats();
        REQUIRE(stats.has_value());
        REQUIRE(stats->min_size == 1);
        REQUIRE(stats->max_size == 3);
        REQUIRE(stats->mean_size == 5.0 / 3.0);
        REQUIRE_THAT(stats->sd_size, Catch::Matchers::WithinAbs(0.943, 0.01));
    }

    SECTION("Modularity setter") {
        cg.set_modularity(0.42);
        REQUIRE(cg.modularity() == 0.42);
    }
}

TEST_CASE("CSR construction and neighbor iteration", "[csr]") {
    Graph<Node, Edge> g;
    g.createNode(NodeId{1}, "a");
    g.createNode(NodeId{2}, "b");
    g.createNode(NodeId{3}, "c");
    auto& e12 = g.createEdge(NodeId{1}, NodeId{2});
    g.connect(e12.id, NodeId{1});
    auto& e23 = g.createEdge(NodeId{2}, NodeId{3});
    g.connect(e23.id, NodeId{2});

    SECTION("Build and basic properties") {
        CSR<Node, Edge> csr;
        csr.build(g);
        REQUIRE(csr.is_valid());
        REQUIRE(csr.num_nodes() == 3);
        REQUIRE(csr.row_offsets().size() == 4);
        REQUIRE(csr.column_indices().size() == 4);
    }

    SECTION("Neighbor iteration") {
        CSR<Node, Edge> csr;
        csr.build(g);

        size_t n0_neighbors = csr.neighbor_count(0);
        REQUIRE(n0_neighbors == 1);

        size_t n1_neighbors = csr.neighbor_count(1);
        REQUIRE(n1_neighbors == 2);

        size_t n2_neighbors = csr.neighbor_count(2);
        REQUIRE(n2_neighbors == 1);
    }

    SECTION("Invalidate flag") {
        CSR<Node, Edge> csr;
        csr.build(g);
        REQUIRE(csr.is_valid());
        csr.invalidate();
        REQUIRE_FALSE(csr.is_valid());
    }

    SECTION("Neighbors of node with 2 connections") {
        CSR<Node, Edge> csr;
        csr.build(g);

        const auto* begin = csr.neighbors_begin(1);
        const auto* end = csr.neighbors_end(1);
        REQUIRE(end - begin == 2);
    }
}

TEST_CASE("Union-Find operations", "[union_find]") {
    SECTION("Initial state: all separate") {
        UnionFind uf(5);
        REQUIRE(uf.count_components() == 5);
        REQUIRE(uf.connected(0, 0));
        REQUIRE_FALSE(uf.connected(0, 1));
    }

    SECTION("Unite and check connectivity") {
        UnionFind uf(5);
        uf.unite(0, 1);
        REQUIRE(uf.connected(0, 1));
        REQUIRE(uf.count_components() == 4);

        uf.unite(2, 3);
        REQUIRE(uf.connected(2, 3));
        REQUIRE(uf.count_components() == 3);

        REQUIRE_FALSE(uf.connected(0, 2));
    }

    SECTION("Unite already connected") {
        UnionFind uf(3);
        uf.unite(0, 1);
        REQUIRE(uf.count_components() == 2);
        uf.unite(0, 1);
        REQUIRE(uf.count_components() == 2);
    }

    SECTION("Transitive connectivity") {
        UnionFind uf(5);
        uf.unite(0, 1);
        uf.unite(1, 2);
        REQUIRE(uf.connected(0, 2));
        REQUIRE(uf.count_components() == 3);
    }

    SECTION("Union by rank") {
        UnionFind uf(4);
        uf.unite(0, 1);
        uf.unite(2, 3);
        uf.unite(0, 2);
        REQUIRE(uf.connected(0, 3));
        REQUIRE(uf.connected(1, 2));
        REQUIRE(uf.count_components() == 1);
    }

    SECTION("Path compression") {
        UnionFind uf(10);
        for (size_t i = 1; i < 10; ++i) {
            uf.unite(0, i);
        }
        REQUIRE(uf.count_components() == 1);
        for (size_t i = 0; i < 10; ++i) {
            REQUIRE(uf.connected(0, i));
        }
    }

    SECTION("Self connection") {
        UnionFind uf(3);
        REQUIRE(uf.connected(0, 0));
        REQUIRE(uf.connected(1, 1));
    }
}

TEST_CASE("DIMACS writer round-trip", "[dimacs]") {
    SECTION("Basic output format") {
        Graph<Node, Edge> g;
        g.createNode(NodeId{1}, "x1");
        g.createNode(NodeId{2}, "x2");

        auto& c1 = g.addClause();
        c1.add_literal(NodeId{1}, true);
        c1.add_literal(NodeId{2}, false);

        auto& c2 = g.addClause();
        c2.add_literal(NodeId{1}, false);

        std::ostringstream oss;
        DimacsWriter<Node, Edge>::write(oss, g);

        std::string output = oss.str();
        REQUIRE(output.find("p cnf 2 2") != std::string::npos);
        REQUIRE(output.find("c 1 x1") != std::string::npos);
        REQUIRE(output.find("c 2 x2") != std::string::npos);
        REQUIRE(output.find("1 -2 0") != std::string::npos);
        REQUIRE(output.find("-1 0") != std::string::npos);
    }

    SECTION("Empty graph") {
        Graph<Node, Edge> g;
        std::ostringstream oss;
        DimacsWriter<Node, Edge>::write(oss, g);
        REQUIRE(oss.str().find("p cnf 0 0") != std::string::npos);
    }

    SECTION("Clauses only, no nodes") {
        Graph<Node, Edge> g;
        g.addClause();
        std::ostringstream oss;
        DimacsWriter<Node, Edge>::write(oss, g);
        REQUIRE(oss.str().find("p cnf 0 1") != std::string::npos);
        REQUIRE(oss.str().find("0\n") != std::string::npos);
    }
}
