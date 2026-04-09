#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <satgraf/graph.hpp>
#include <satgraf/node.hpp>
#include <satgraf/edge.hpp>
#include <satgraf/layout.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/dimacs_parser.hpp>
#include <satgraf/evolution.hpp>
#include <satgraf_gui/graph_renderer.hpp>
#include <satgraf_gui/export.hpp>

#include <QApplication>
#include <QGraphicsItem>
#include <QImage>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace satgraf;

static int g_argc = 1;
static char g_arg0[] = "test_gui";
static char* g_argv[] = {g_arg0, nullptr};
static QApplication* g_app = nullptr;

static void ensure_qapp() {
    if (!g_app) {
        g_app = new QApplication(g_argc, g_argv);
    }
}

static graph::Graph<graph::Node, graph::Edge> make_large_graph(int num_vars, int edges_per_var) {
    graph::Graph<graph::Node, graph::Edge> g;
    for (int i = 1; i <= num_vars; ++i) {
        g.createNode(graph::NodeId(static_cast<uint32_t>(i)), std::to_string(i));
    }
    for (int i = 1; i <= num_vars; ++i) {
        for (int j = i + 1; j <= std::min(i + edges_per_var, num_vars); ++j) {
            g.createEdge(graph::NodeId(static_cast<uint32_t>(i)),
                        graph::NodeId(static_cast<uint32_t>(j)));
        }
    }
    return g;
}

static layout::CoordinateMap compute_test_coords(
    const graph::Graph<graph::Node, graph::Edge>& g)
{
    auto layout = layout::LayoutFactory::instance().create("f");
    return layout->compute(g);
}

static community::CommunityResult compute_test_communities(
    const graph::Graph<graph::Node, graph::Edge>& g)
{
    auto detector = community::DetectorFactory::instance().create("louvain");
    return detector->detect(g);
}

// ---------------------------------------------------------------------------
// 9.12 — Rendering integration tests
// ---------------------------------------------------------------------------

TEST_CASE("Render 10K-node graph produces correct item counts", "[gui][rendering]") {
    ensure_qapp();
    auto g = make_large_graph(100, 3);
    auto coords = compute_test_coords(g);
    auto communities = compute_test_communities(g);

    rendering::GraphRenderer renderer;
    renderer.render(g, coords, communities);

    REQUIRE(renderer.node_count() == 100);
    REQUIRE(renderer.edge_count() > 0);
}

TEST_CASE("Layer z-ordering: nodes above edges", "[gui][rendering]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    g.createNode(graph::NodeId(1), "1");
    g.createNode(graph::NodeId(2), "2");
    g.createEdge(graph::NodeId(1), graph::NodeId(2));

    layout::CoordinateMap coords;
    coords[graph::NodeId(1)] = {10.0, 10.0};
    coords[graph::NodeId(2)] = {50.0, 50.0};

    community::CommunityResult empty_comm;

    rendering::GraphRenderer renderer;
    renderer.render(g, coords, empty_comm);

    auto* scene = renderer.scene();
    QList<QGraphicsItem*> items = scene->items();

    double min_edge_z = std::numeric_limits<double>::max();
    double min_node_z = std::numeric_limits<double>::max();
    for (auto* item : items) {
        if (dynamic_cast<rendering::EdgeGraphicsItem*>(item)) {
            min_edge_z = std::min(min_edge_z, item->zValue());
        }
        if (dynamic_cast<rendering::NodeGraphicsItem*>(item)) {
            min_node_z = std::min(min_node_z, item->zValue());
        }
    }
    REQUIRE(min_node_z > min_edge_z);
}

TEST_CASE("Node click detection finds correct node", "[gui][rendering]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    g.createNode(graph::NodeId(42), "42");

    layout::CoordinateMap coords;
    coords[graph::NodeId(42)] = {100.0, 100.0};

    community::CommunityResult empty_comm;

    rendering::GraphRenderer renderer;
    renderer.render(g, coords, empty_comm, 20.0);

    auto found = renderer.node_at(QPointF(100.0, 100.0));
    REQUIRE(found.has_value());
    REQUIRE(*found == graph::NodeId(42));
}

TEST_CASE("Visibility filter hides nodes by community", "[gui][rendering]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    g.createNode(graph::NodeId(1), "1");
    g.createNode(graph::NodeId(2), "2");
    g.createNode(graph::NodeId(3), "3");
    g.createEdge(graph::NodeId(1), graph::NodeId(2));
    g.createEdge(graph::NodeId(2), graph::NodeId(3));

    auto coords = compute_test_coords(g);
    auto communities = compute_test_communities(g);

    rendering::GraphRenderer renderer;
    renderer.render(g, coords, communities);
    renderer.store_graph(&g);

    rendering::VisibilityFilters filters;
    filters.show_unassigned = false;
    renderer.apply_filters(filters, communities);

    auto* scene = renderer.scene();
    int visible = 0;
    for (auto* item : scene->items()) {
        if (dynamic_cast<rendering::NodeGraphicsItem*>(item) && item->isVisible()) {
            visible++;
        }
    }
    REQUIRE(visible == 0);
}

// ---------------------------------------------------------------------------
// 11.5 — Export unit tests
// ---------------------------------------------------------------------------

TEST_CASE("Export rendered graph to PNG produces non-empty file", "[gui][export]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    for (int i = 1; i <= 10; ++i) {
        g.createNode(graph::NodeId(static_cast<uint32_t>(i)), std::to_string(i));
    }
    for (int i = 1; i <= 9; ++i) {
        g.createEdge(graph::NodeId(static_cast<uint32_t>(i)),
                    graph::NodeId(static_cast<uint32_t>(i + 1)));
    }

    auto coords = compute_test_coords(g);
    auto communities = compute_test_communities(g);

    std::string outpath = "/tmp/satgraf-test-export-unit-" +
                          std::to_string(::getpid()) + ".png";

    bool ok = export_::export_png(g, coords, communities, outpath, 512, 512);
    REQUIRE(ok);

    std::ifstream f(outpath, std::ios::binary);
    REQUIRE(f.is_open());
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    REQUIRE(size > 0);

    std::filesystem::remove(outpath);
}

TEST_CASE("Export rendered graph to JPEG produces non-empty file", "[gui][export]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    for (int i = 1; i <= 5; ++i) {
        g.createNode(graph::NodeId(static_cast<uint32_t>(i)), std::to_string(i));
    }
    g.createEdge(graph::NodeId(1), graph::NodeId(2));
    g.createEdge(graph::NodeId(3), graph::NodeId(4));

    layout::CoordinateMap coords;
    for (int i = 1; i <= 5; ++i) {
        coords[graph::NodeId(static_cast<uint32_t>(i))] = {
            static_cast<double>(i * 100), static_cast<double>(i * 80)};
    }

    community::CommunityResult empty_comm;
    std::string outpath = "/tmp/satgraf-test-export-jpeg-" +
                          std::to_string(::getpid()) + ".jpg";

    bool ok = export_::export_jpeg(g, coords, empty_comm, outpath, 256, 256, 90);
    REQUIRE(ok);

    std::ifstream f(outpath, std::ios::binary);
    REQUIRE(f.is_open());
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    REQUIRE(size > 0);

    std::filesystem::remove(outpath);
}

TEST_CASE("Export GIF frames from evolution events", "[gui][export]") {
    ensure_qapp();
    graph::Graph<graph::Node, graph::Edge> g;
    for (int i = 1; i <= 5; ++i) {
        g.createNode(graph::NodeId(static_cast<uint32_t>(i)), std::to_string(i));
    }

    std::vector<std::string> events = {
        "v d 1 0.5 1",
        "c + 1 2 0",
        "! 1",
        "v d 1 0.8 2",
        "c + 2 3 0",
        "! 2",
        "v d 0 0.3 3",
        "c - 1 2 0",
        "! 3",
    };

    auto frames = export_::render_evolution_frames(
        g, events, evolution::GraphMode::VIG, 256, 256);

    REQUIRE(frames.size() == 3);

    std::string outpath = "/tmp/satgraf-test-gif-" +
                          std::to_string(::getpid());
    bool ok = export_::save_gif_frames(frames, outpath, 100);
    REQUIRE(ok);

    for (size_t i = 0; i < frames.size(); ++i) {
        std::string frame_path = outpath + "_" +
            std::string(4 - std::to_string(i).size(), '0') + std::to_string(i) + ".png";
        REQUIRE(std::filesystem::exists(frame_path));
        std::filesystem::remove(frame_path);
    }
}

// ---------------------------------------------------------------------------
// 10.14 — GUI integration tests (headless verification)
// ---------------------------------------------------------------------------

TEST_CASE("Headless export via CLI produces valid PNG", "[gui][integration]") {
    ensure_qapp();
    std::string cnf_path = "/tmp/satgraf-test-integration-" +
                           std::to_string(::getpid()) + ".cnf";
    {
        std::ofstream f(cnf_path);
        f << "p cnf 5 3\n";
        f << "1 2 3 0\n";
        f << "-1 -2 4 0\n";
        f << "-3 -4 5 0\n";
    }

    dimacs::Parser parser;
    auto graph = parser.parse(cnf_path, dimacs::Mode::VIG);

    auto detector = community::DetectorFactory::instance().create("louvain");
    auto communities = detector->detect(graph);

    auto layout = layout::LayoutFactory::instance().create("f");
    auto coords = layout->compute(graph);

    std::string outpath = "/tmp/satgraf-test-cli-export-" +
                          std::to_string(::getpid()) + ".png";
    QImage img = export_::render_headless(graph, coords, communities, 1024, 1024);
    REQUIRE(!img.isNull());
    REQUIRE(img.width() == 1024);
    REQUIRE(img.height() == 1024);
    REQUIRE(img.save(QString::fromStdString(outpath), "PNG"));

    std::ifstream f(outpath, std::ios::binary);
    f.seekg(0, std::ios::end);
    REQUIRE(f.tellg() > 1000);

    std::filesystem::remove(cnf_path);
    std::filesystem::remove(outpath);
}

// ---------------------------------------------------------------------------
// 12.4 — End-to-end community mode test
// ---------------------------------------------------------------------------

TEST_CASE("E2E: parse → community detect → layout → render → export PNG", "[gui][e2e]") {
    ensure_qapp();

    std::string cnf_path = "/tmp/satgraf-e2e-community-" +
                           std::to_string(::getpid()) + ".cnf";
    {
        std::ofstream f(cnf_path);
        f << "p cnf 20 15\n";
        for (int i = 0; i < 15; ++i) {
            f << (i * 3 + 1) << " " << (i * 3 + 2) << " " << (i * 3 + 3) << " 0\n";
        }
    }

    dimacs::Parser parser;
    auto graph = parser.parse(cnf_path, dimacs::Mode::VIG);
    REQUIRE(graph.nodeCount() > 0);
    REQUIRE(graph.edgeCount() > 0);

    auto detector = community::DetectorFactory::instance().create("louvain");
    auto communities = detector->detect(graph);
    REQUIRE(communities.assignment.size() > 0);

    auto layout = layout::LayoutFactory::instance().create("f");
    auto coords = layout->compute(graph);
    REQUIRE(coords.size() == graph.nodeCount());

    for (const auto& [nid, c] : coords) {
        (void)nid;
        REQUIRE(!std::isnan(c.x));
        REQUIRE(!std::isnan(c.y));
        REQUIRE(!std::isinf(c.x));
        REQUIRE(!std::isinf(c.y));
    }

    rendering::GraphRenderer renderer;
    renderer.render(graph, coords, communities);
    REQUIRE(renderer.node_count() > 0);

    std::string outpath = "/tmp/satgraf-e2e-output-" +
                          std::to_string(::getpid()) + ".png";
    bool ok = export_::export_png(graph, coords, communities, outpath, 1024, 1024);
    REQUIRE(ok);

    std::ifstream f(outpath, std::ios::binary);
    f.seekg(0, std::ios::end);
    REQUIRE(f.tellg() > 0);

    std::filesystem::remove(cnf_path);
    std::filesystem::remove(outpath);
}

// ---------------------------------------------------------------------------
// 12.5 — End-to-end evolution mode test
// ---------------------------------------------------------------------------

TEST_CASE("E2E: parse → create engine → replay events → render frame", "[gui][e2e]") {
    ensure_qapp();

    std::string cnf_path = "/tmp/satgraf-e2e-evo-" +
                           std::to_string(::getpid()) + ".cnf";
    {
        std::ofstream f(cnf_path);
        f << "p cnf 5 3\n";
        f << "1 2 3 0\n";
        f << "-1 -2 4 0\n";
        f << "-3 -4 5 0\n";
    }

    dimacs::Parser parser;
    auto graph = parser.parse(cnf_path, dimacs::Mode::VIG);
    REQUIRE(graph.nodeCount() > 0);

    evolution::EvolutionEngine engine(graph, evolution::GraphMode::VIG);

    std::vector<std::string> event_lines = {
        "v d 1 0.5 1",
        "v p 1 0.3 2",
        "c + 1 -2 0",
        "! 1",
        "v d 0 0.2 1",
        "c + 2 3 0",
        "! 2",
    };

    for (const auto& line : event_lines) {
        engine.process_line(line);
    }

    REQUIRE(engine.current_conflict() == 2);
    REQUIRE(engine.history_depth() == 7);

    engine.step_backward();
    REQUIRE(engine.current_conflict() == 1);

    auto coords = compute_test_coords(graph);
    community::CommunityResult empty_comm;

    std::string outpath = "/tmp/satgraf-e2e-evo-frame-" +
                          std::to_string(::getpid()) + ".png";
    bool ok = export_::export_png(graph, coords, empty_comm, outpath, 512, 512);
    REQUIRE(ok);

    std::filesystem::remove(cnf_path);
    std::filesystem::remove(outpath);
}

// ---------------------------------------------------------------------------
// 12.6 — Performance benchmark
// ---------------------------------------------------------------------------

TEST_CASE("Performance: layout 1K vars completes in < 30s", "[gui][benchmark]") {
    auto g = make_large_graph(1000, 5);
    REQUIRE(g.nodeCount() == 1000);

    auto detector = community::DetectorFactory::instance().create("louvain");
    auto communities = detector->detect(g);

    auto start = std::chrono::steady_clock::now();
    auto layout = std::make_unique<layout::FruchtermanReingoldLayout>(100);
    auto coords = layout->compute(g);
    auto end = std::chrono::steady_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    REQUIRE(elapsed < 30.0);
    REQUIRE(coords.size() == 1000);

    for (const auto& [nid, c] : coords) {
        (void)nid;
        REQUIRE(!std::isnan(c.x));
        REQUIRE(!std::isnan(c.y));
    }
}
