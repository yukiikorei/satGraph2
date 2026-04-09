#include <catch2/catch_test_macros.hpp>

#include "satgraf/dimacs_parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path data_dir() {
    return std::filesystem::path(__FILE__).parent_path() / "data";
}

std::string data_file(const std::string& name) {
    return (data_dir() / name).string();
}

struct TempFile {
    std::string path;
    explicit TempFile(const std::string& content) {
        static int counter = 0;
        auto p = std::filesystem::temp_directory_path() /
                 ("dimacs_test_" + std::to_string(++counter) + ".cnf");
        path = p.string();
        std::ofstream f(path);
        f << content;
    }
    ~TempFile() { std::filesystem::remove(path); }
};

double find_edge_weight(const satgraf::graph::Graph<satgraf::graph::Node,
                                                satgraf::graph::Edge>& g,
                        uint32_t a, uint32_t b) {
    const auto key = std::minmax(a, b);
    for (const auto& [eid, edge] : g.getEdges()) {
        const auto ek = std::minmax(
            static_cast<uint32_t>(edge.source),
            static_cast<uint32_t>(edge.target));
        if (ek == key) return edge.weight;
    }
    return 0.0;
}

bool has_edge(const satgraf::graph::Graph<satgraf::graph::Node,
                                        satgraf::graph::Edge>& g,
              uint32_t a, uint32_t b) {
    return find_edge_weight(g, a, b) > 0.0;
}

} // anonymous namespace

using namespace satgraf::dimacs;
using namespace satgraf::graph;

// ---------------------------------------------------------------
// VIG mode
// ---------------------------------------------------------------

TEST_CASE("VIG parses simple.cnf with correct structure", "[dimacs][vig]") {
    Parser parser;
    const auto g = parser.parse(data_file("simple.cnf"), Mode::VIG);

    REQUIRE(g.nodeCount() == 5);
    REQUIRE(g.getClauses().size() == 3);

    SECTION("correct edge count") {
        REQUIRE(g.edgeCount() == 9);
    }

    SECTION("edge weights reflect co-occurrence counts") {
        REQUIRE(find_edge_weight(g, 1, 2) == 2.0);
        REQUIRE(find_edge_weight(g, 1, 3) == 2.0);
        REQUIRE(find_edge_weight(g, 1, 4) == 2.0);
        REQUIRE(find_edge_weight(g, 2, 3) == 1.0);
        REQUIRE(find_edge_weight(g, 2, 4) == 1.0);
        REQUIRE(find_edge_weight(g, 1, 5) == 1.0);
        REQUIRE(find_edge_weight(g, 3, 4) == 1.0);
        REQUIRE(find_edge_weight(g, 3, 5) == 1.0);
        REQUIRE(find_edge_weight(g, 4, 5) == 1.0);
    }

    SECTION("non-existent edge returns zero weight") {
        REQUIRE_FALSE(has_edge(g, 2, 5));
    }

    SECTION("node ids match DIMACS variable convention") {
        for (uint32_t v = 1; v <= 5; ++v) {
            REQUIRE(g.getNode(NodeId(v)).has_value());
        }
    }
}

// ---------------------------------------------------------------
// LIG mode
// ---------------------------------------------------------------

TEST_CASE("LIG parses simple.cnf with correct signed nodes", "[dimacs][lig]") {
    Parser parser;
    const auto g = parser.parse(data_file("simple.cnf"), Mode::LIG);

    REQUIRE(g.getClauses().size() == 3);

    SECTION("correct node count — 9 unique signed literals") {
        REQUIRE(g.nodeCount() == 9);
    }

    SECTION("correct edge count — 12 unique literal pairs") {
        REQUIRE(g.edgeCount() == 12);
    }

    SECTION("node names encode sign and variable") {
        REQUIRE(g.getNode(NodeId(2))->get().name == "+1");
        REQUIRE(g.getNode(NodeId(3))->get().name == "-1");
        REQUIRE(g.getNode(NodeId(4))->get().name == "+2");
        REQUIRE(g.getNode(NodeId(5))->get().name == "-2");
        REQUIRE(g.getNode(NodeId(6))->get().name == "+3");
        REQUIRE(g.getNode(NodeId(7))->get().name == "-3");
        REQUIRE(g.getNode(NodeId(8))->get().name == "+4");
        REQUIRE(g.getNode(NodeId(9))->get().name == "-4");
        REQUIRE(g.getNode(NodeId(10))->get().name == "+5");
    }

    SECTION("all LIG edges have weight 1 — no literal pair co-occurs twice") {
        for (const auto& [eid, edge] : g.getEdges()) {
            REQUIRE(edge.weight == 1.0);
        }
    }

    SECTION("edges from clause 1 (+1,+2,+3)") {
        REQUIRE(has_edge(g, 2, 4));
        REQUIRE(has_edge(g, 2, 6));
        REQUIRE(has_edge(g, 4, 6));
    }

    SECTION("edges from clause 2 (-1,-2,+4)") {
        REQUIRE(has_edge(g, 3, 5));
        REQUIRE(has_edge(g, 3, 8));
        REQUIRE(has_edge(g, 5, 8));
    }

    SECTION("edges from clause 3 (+1,-3,-4,+5)") {
        REQUIRE(has_edge(g, 2, 7));
        REQUIRE(has_edge(g, 2, 9));
        REQUIRE(has_edge(g, 2, 10));
        REQUIRE(has_edge(g, 7, 9));
        REQUIRE(has_edge(g, 7, 10));
        REQUIRE(has_edge(g, 9, 10));
    }
}

// ---------------------------------------------------------------
// p-line validation
// ---------------------------------------------------------------

TEST_CASE("Missing p-line throws runtime_error", "[dimacs][pline]") {
    TempFile tmp("1 2 3 0\n");
    Parser parser;
    REQUIRE_THROWS_AS(parser.parse(tmp.path, Mode::VIG), std::runtime_error);
}

TEST_CASE("Missing p-line error message contains context", "[dimacs][pline]") {
    TempFile tmp("1 2 3 0\n");
    Parser parser;
    try {
        parser.parse(tmp.path, Mode::VIG);
        FAIL("Expected std::runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        REQUIRE(msg.find("Clause data before p-line") != std::string::npos);
        REQUIRE(msg.find("line 1") != std::string::npos);
    }
}

TEST_CASE("Duplicate p-line throws runtime_error", "[dimacs][pline]") {
    TempFile tmp("p cnf 3 2\np cnf 3 2\n1 2 0\n");
    Parser parser;
    REQUIRE_THROWS_AS(parser.parse(tmp.path, Mode::VIG), std::runtime_error);
}

TEST_CASE("Duplicate p-line error message mentions duplicate", "[dimacs][pline]") {
    TempFile tmp("p cnf 3 2\np cnf 3 2\n1 2 0\n");
    Parser parser;
    try {
        parser.parse(tmp.path, Mode::VIG);
        FAIL("Expected std::runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        REQUIRE(msg.find("Duplicate p-line") != std::string::npos);
        REQUIRE(msg.find("line 2") != std::string::npos);
    }
}

// ---------------------------------------------------------------
// c-line variable naming
// ---------------------------------------------------------------

TEST_CASE("c-line assigns variable names in VIG mode", "[dimacs][cline]") {
    Parser parser;
    const auto g = parser.parse(data_file("named.cnf"), Mode::VIG);

    REQUIRE(g.getNode(NodeId(1))->get().name == "alpha");
    REQUIRE(g.getNode(NodeId(2))->get().name == "beta");
    REQUIRE(g.getNode(NodeId(3))->get().name == "gamma");
}

TEST_CASE("c-line assigns signed names in LIG mode", "[dimacs][cline]") {
    Parser parser;
    const auto g = parser.parse(data_file("named.cnf"), Mode::LIG);

    REQUIRE(g.getNode(NodeId(2))->get().name == "+alpha");
    REQUIRE(g.getNode(NodeId(3))->get().name == "-alpha");
    REQUIRE(g.getNode(NodeId(4))->get().name == "+beta");
    REQUIRE(g.getNode(NodeId(7))->get().name == "-gamma");
}

TEST_CASE("Plain comment lines without numeric id are skipped", "[dimacs][cline]") {
    TempFile tmp(
        "c This is a comment line with no variable id\n"
        "p cnf 2 1\n"
        "1 2 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);
    REQUIRE(g.nodeCount() == 2);
    REQUIRE(g.getNode(NodeId(1))->get().name == "1");
    REQUIRE(g.getNode(NodeId(2))->get().name == "2");
}

TEST_CASE("c-line after clause data updates existing node name", "[dimacs][cline]") {
    TempFile tmp(
        "p cnf 2 1\n"
        "1 2 0\n"
        "c 1 late_name\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);
    REQUIRE(g.getNode(NodeId(1))->get().name == "late_name");
}

// ---------------------------------------------------------------
// Regex-based grouping
// ---------------------------------------------------------------

TEST_CASE("Regex grouping assigns groups to matched variables", "[dimacs][grouping]") {
    TempFile tmp(
        "p cnf 4 2\n"
        "c 1 foo_bar\n"
        "c 2 baz_qux\n"
        "c 3 hello_world\n"
        "c 4 nomatch\n"
        "1 2 0\n"
        "3 4 0\n");

    std::vector<std::regex> patterns;
    patterns.emplace_back("(.+)_(.+)");

    Parser parser(std::move(patterns));
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getNode(NodeId(1))->get().groups.size() == 1);
    REQUIRE(g.getNode(NodeId(1))->get().groups[0] == "foo");

    REQUIRE(g.getNode(NodeId(2))->get().groups[0] == "baz");
    REQUIRE(g.getNode(NodeId(3))->get().groups[0] == "hello");

    REQUIRE(g.getNode(NodeId(4))->get().groups[0] == "ungrouped");
}

TEST_CASE("Regex grouping with no capture group uses full match", "[dimacs][grouping]") {
    TempFile tmp(
        "p cnf 2 1\n"
        "c 1 prefix_alpha\n"
        "c 2 other_thing\n"
        "1 2 0\n");

    std::vector<std::regex> patterns;
    patterns.emplace_back("prefix_\\w+");

    Parser parser(std::move(patterns));
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getNode(NodeId(1))->get().groups[0] == "prefix_alpha");
    REQUIRE(g.getNode(NodeId(2))->get().groups[0] == "ungrouped");
}

TEST_CASE("First matching regex wins", "[dimacs][grouping]") {
    TempFile tmp(
        "p cnf 1 1\n"
        "c 1 abc_def\n"
        "1 0\n");

    std::vector<std::regex> patterns;
    patterns.emplace_back("(\\w+)_(\\w+)");
    patterns.emplace_back("abc");

    Parser parser(std::move(patterns));
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getNode(NodeId(1))->get().groups[0] == "abc");
}

TEST_CASE("No regex patterns means no groups assigned", "[dimacs][grouping]") {
    Parser parser;
    const auto g = parser.parse(data_file("simple.cnf"), Mode::VIG);

    for (const auto& [nid, node] : g.getNodes()) {
        REQUIRE(node.groups.empty());
    }
}

TEST_CASE("LIG grouping uses underlying variable name", "[dimacs][grouping]") {
    TempFile tmp(
        "p cnf 2 1\n"
        "c 1 groupA_var1\n"
        "c 2 groupB_var2\n"
        "1 -2 0\n");

    std::vector<std::regex> patterns;
    patterns.emplace_back("(group\\w+)_");

    Parser parser(std::move(patterns));
    const auto g = parser.parse(tmp.path, Mode::LIG);

    REQUIRE(g.getNode(NodeId(2))->get().groups[0] == "groupA");
    REQUIRE(g.getNode(NodeId(5))->get().groups[0] == "groupB");
}

// ---------------------------------------------------------------
// Clause parsing edge cases
// ---------------------------------------------------------------

TEST_CASE("Multi-line clause is parsed correctly", "[dimacs][clauses]") {
    Parser parser;
    const auto g = parser.parse(data_file("multiline.cnf"), Mode::VIG);

    REQUIRE(g.getClauses().size() == 2);
    REQUIRE(g.nodeCount() == 4);

    SECTION("first clause spans two lines") {
        const auto& c = g.getClauses()[0];
        REQUIRE(c.size() == 3);
        REQUIRE(c.has_literal(NodeId(1)));
        REQUIRE(c.has_literal(NodeId(2)));
        REQUIRE(c.has_literal(NodeId(3)));
    }

    SECTION("second clause on single line") {
        const auto& c = g.getClauses()[1];
        REQUIRE(c.size() == 4);
        REQUIRE(c.get_polarity(NodeId(1)) == false);
        REQUIRE(c.get_polarity(NodeId(2)) == false);
        REQUIRE(c.get_polarity(NodeId(3)) == false);
        REQUIRE(c.get_polarity(NodeId(4)) == true);
    }

    SECTION("VIG edges with correct weights") {
        REQUIRE(find_edge_weight(g, 1, 2) == 2.0);
        REQUIRE(find_edge_weight(g, 1, 3) == 2.0);
        REQUIRE(find_edge_weight(g, 2, 3) == 2.0);
        REQUIRE(find_edge_weight(g, 1, 4) == 1.0);
        REQUIRE(find_edge_weight(g, 2, 4) == 1.0);
        REQUIRE(find_edge_weight(g, 3, 4) == 1.0);
        REQUIRE(g.edgeCount() == 6);
    }
}

TEST_CASE("Unit clause parsed correctly", "[dimacs][clauses]") {
    TempFile tmp("p cnf 3 3\n1 0\n2 0\n-3 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getClauses().size() == 3);
    REQUIRE(g.nodeCount() == 3);

    SECTION("no edges from unit clauses") {
        REQUIRE(g.edgeCount() == 0);
    }

    SECTION("clause literals recorded") {
        REQUIRE(g.getClauses()[0].size() == 1);
        REQUIRE(g.getClauses()[0].has_literal(NodeId(1)));
        REQUIRE(g.getClauses()[0].get_polarity(NodeId(1)) == true);

        REQUIRE(g.getClauses()[1].size() == 1);
        REQUIRE(g.getClauses()[1].has_literal(NodeId(2)));

        REQUIRE(g.getClauses()[2].get_polarity(NodeId(3)) == false);
    }
}

TEST_CASE("Empty clause is recorded", "[dimacs][clauses]") {
    TempFile tmp("p cnf 2 3\n1 2 0\n0\n-1 -2 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getClauses().size() == 3);

    SECTION("middle clause is empty") {
        REQUIRE(g.getClauses()[1].empty());
    }

    SECTION("other clauses are intact") {
        REQUIRE(g.getClauses()[0].size() == 2);
        REQUIRE(g.getClauses()[2].size() == 2);
    }

    SECTION("edge weight counts clauses with pairs") {
        REQUIRE(find_edge_weight(g, 1, 2) == 2.0);
        REQUIRE(g.edgeCount() == 1);
    }
}

TEST_CASE("Unterminated clause at EOF is treated as complete", "[dimacs][clauses]") {
    TempFile tmp("p cnf 2 1\n1 2");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getClauses().size() == 1);
    REQUIRE(g.getClauses()[0].size() == 2);
    REQUIRE(g.edgeCount() == 1);
}

TEST_CASE("Multiple clauses on a single line", "[dimacs][clauses]") {
    TempFile tmp("p cnf 3 2\n1 2 0 2 3 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.getClauses().size() == 2);
    REQUIRE(g.edgeCount() == 2);
    REQUIRE(find_edge_weight(g, 1, 2) == 1.0);
    REQUIRE(find_edge_weight(g, 2, 3) == 1.0);
    REQUIRE_FALSE(has_edge(g, 1, 3));
}

// ---------------------------------------------------------------
// Progress callback
// ---------------------------------------------------------------

TEST_CASE("Progress callback receives increasing values", "[dimacs][progress]") {
    std::vector<double> values;
    Parser parser({}, [&values](double p) { values.push_back(p); });
    const auto g = parser.parse(data_file("simple.cnf"), Mode::VIG);

    REQUIRE_FALSE(values.empty());

    SECTION("values are monotonically non-decreasing") {
        for (std::size_t i = 1; i < values.size(); ++i) {
            REQUIRE(values[i] >= values[i - 1]);
        }
    }

    SECTION("final value is 1.0") {
        REQUIRE(values.back() == 1.0);
    }

    SECTION("all values in [0, 1]") {
        for (const double v : values) {
            REQUIRE(v >= 0.0);
            REQUIRE(v <= 1.0);
        }
    }
}

TEST_CASE("No progress callback does not crash", "[dimacs][progress]") {
    Parser parser;
    const auto g = parser.parse(data_file("simple.cnf"), Mode::VIG);
    REQUIRE(g.nodeCount() == 5);
}

// ---------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------

TEST_CASE("Invalid literal throws with line number and token", "[dimacs][errors]") {
    TempFile tmp("p cnf 3 1\n1 abc 0\n");
    Parser parser;
    try {
        parser.parse(tmp.path, Mode::VIG);
        FAIL("Expected std::runtime_error");
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        REQUIRE(msg.find("Invalid literal") != std::string::npos);
        REQUIRE(msg.find("abc") != std::string::npos);
        REQUIRE(msg.find("line 2") != std::string::npos);
    }
}

TEST_CASE("File not found throws runtime_error", "[dimacs][errors]") {
    Parser parser;
    REQUIRE_THROWS_AS(
        parser.parse("/nonexistent/path/file.cnf", Mode::VIG),
        std::runtime_error);
}

TEST_CASE("Literal with trailing non-numeric chars throws", "[dimacs][errors]") {
    TempFile tmp("p cnf 2 1\n1 2abc 0\n");
    Parser parser;
    REQUIRE_THROWS_AS(parser.parse(tmp.path, Mode::VIG), std::runtime_error);
}

// ---------------------------------------------------------------
// Empty / minimal inputs
// ---------------------------------------------------------------

TEST_CASE("File with only p-line returns empty graph", "[dimacs][minimal]") {
    TempFile tmp("p cnf 0 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.nodeCount() == 0);
    REQUIRE(g.edgeCount() == 0);
    REQUIRE(g.getClauses().empty());
}

TEST_CASE("File with only comments and p-line returns empty graph", "[dimacs][minimal]") {
    TempFile tmp(
        "c A comment\n"
        "c Another comment\n"
        "p cnf 10 0\n");
    Parser parser;
    const auto g = parser.parse(tmp.path, Mode::VIG);

    REQUIRE(g.nodeCount() == 0);
    REQUIRE(g.edgeCount() == 0);
}
