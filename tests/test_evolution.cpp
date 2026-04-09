#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <satgraf/evolution.hpp>
#include <satgraf/graph.hpp>
#include <satgraf/node.hpp>
#include <satgraf/edge.hpp>
#include <fstream>
#include <filesystem>
#include <thread>
#include <unistd.h>

using namespace satgraf::evolution;
using namespace satgraf::graph;

namespace {

Graph<Node, Edge> make_test_graph(int num_vars) {
    Graph<Node, Edge> g;
    for (int i = 1; i <= num_vars; ++i) {
        g.createNode(NodeId(static_cast<uint32_t>(i)), std::to_string(i));
    }
    return g;
}

class TestObserver : public EvolutionObserver {
public:
    struct NodeAssignCall {
        NodeId var;
        int state;
        double activity;
    };
    struct ClauseCall {
        std::vector<int> literals;
    };

    std::vector<NodeAssignCall> node_assigned_calls;
    std::vector<ClauseCall> clause_added_calls;
    std::vector<ClauseCall> clause_removed_calls;
    std::vector<int> conflict_calls;
    int update_graph_count{0};
    std::vector<std::string> new_file_calls;
    std::vector<NodeId> decision_var_calls;

    void node_assigned(NodeId var, int state, double activity) override {
        node_assigned_calls.push_back({var, state, activity});
    }
    void clause_added(const std::vector<int>& literals) override {
        clause_added_calls.push_back({literals});
    }
    void clause_removed(const std::vector<int>& literals) override {
        clause_removed_calls.push_back({literals});
    }
    void conflict(int conflict_num) override {
        conflict_calls.push_back(conflict_num);
    }
    void update_graph() override {
        update_graph_count++;
    }
    void new_file_ready(const std::string& filename) override {
        new_file_calls.push_back(filename);
    }
    void decision_variable(NodeId var) override {
        decision_var_calls.push_back(var);
    }
};

}

TEST_CASE("parse_event_line parses variable assignment", "[evolution]") {
    auto ev = parse_event_line("v 42 1 0.85");
    REQUIRE(ev.has_value());
    auto* va = std::get_if<VariableAssignment>(&*ev);
    REQUIRE(va != nullptr);
    REQUIRE(va->var == NodeId(42));
    REQUIRE(va->state == 1);
    REQUIRE(va->activity == Catch::Approx(0.85));
}

TEST_CASE("parse_event_line parses variable unassignment", "[evolution]") {
    auto ev = parse_event_line("v 42 0 0.5");
    REQUIRE(ev.has_value());
    auto* va = std::get_if<VariableAssignment>(&*ev);
    REQUIRE(va != nullptr);
    REQUIRE(va->var == NodeId(42));
    REQUIRE(va->state == 0);
}

TEST_CASE("parse_event_line parses clause addition", "[evolution]") {
    auto ev = parse_event_line("c + 1 -3 5 0");
    REQUIRE(ev.has_value());
    auto* ce = std::get_if<ClauseEvent>(&*ev);
    REQUIRE(ce != nullptr);
    REQUIRE(ce->action == ClauseAction::Add);
    REQUIRE(ce->literals == std::vector<int>{1, -3, 5});
}

TEST_CASE("parse_event_line parses clause removal", "[evolution]") {
    auto ev = parse_event_line("c - 1 -3 5 0");
    REQUIRE(ev.has_value());
    auto* ce = std::get_if<ClauseEvent>(&*ev);
    REQUIRE(ce != nullptr);
    REQUIRE(ce->action == ClauseAction::Remove);
}

TEST_CASE("parse_event_line parses conflict", "[evolution]") {
    auto ev = parse_event_line("! 47");
    REQUIRE(ev.has_value());
    auto* cf = std::get_if<ConflictEvent>(&*ev);
    REQUIRE(cf != nullptr);
    REQUIRE(cf->conflict_num == 47);
}

TEST_CASE("parse_event_line rejects invalid lines", "[evolution]") {
    REQUIRE_FALSE(parse_event_line("").has_value());
    REQUIRE_FALSE(parse_event_line("x garbage").has_value());
    REQUIRE_FALSE(parse_event_line("v incomplete").has_value());
}

TEST_CASE("parse_pipe_event parses v d|p format", "[evolution]") {
    auto ev = parse_pipe_event("v d 1 0.75 15");
    REQUIRE(ev.has_value());
    auto* va = std::get_if<VariableAssignment>(&*ev);
    REQUIRE(va != nullptr);
    REQUIRE(va->var == NodeId(15));
    REQUIRE(va->state == 1);
    REQUIRE(va->activity == Catch::Approx(0.75));
}

TEST_CASE("parse_pipe_event parses propagation", "[evolution]") {
    auto ev = parse_pipe_event("v p 1 0.3 7");
    REQUIRE(ev.has_value());
    auto* va = std::get_if<VariableAssignment>(&*ev);
    REQUIRE(va != nullptr);
    REQUIRE(va->var == NodeId(7));
}

TEST_CASE("Forward evolution applies variable assignments", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});
    auto& node = g.getNode(NodeId(1))->get();
    REQUIRE(node.assignment == Assignment::True);
    REQUIRE(node.activity == Catch::Approx(0.5));
}

TEST_CASE("Forward evolution creates edges for clause addition", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    size_t initial_edges = g.edgeCount();

    engine.process_event(ClauseEvent{ClauseAction::Add, {1, -3, 5}});
    REQUIRE(g.edgeCount() == initial_edges + 3);  // 3 pairs: (1,3), (1,5), (3,5)
}

TEST_CASE("Forward evolution removes edges for clause removal", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ClauseEvent{ClauseAction::Add, {1, 3}});
    size_t after_add = g.edgeCount();
    REQUIRE(after_add > 0);

    engine.process_event(ClauseEvent{ClauseAction::Remove, {1, 3}});
    REQUIRE(g.edgeCount() == 0);
}

TEST_CASE("Forward evolution tracks conflicts", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ConflictEvent{1});
    REQUIRE(engine.current_conflict() == 1);

    engine.process_event(ConflictEvent{5});
    REQUIRE(engine.current_conflict() == 5);
}

TEST_CASE("Backward evolution reverts variable assignment", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});
    REQUIRE(g.getNode(NodeId(1))->get().assignment == Assignment::True);

    bool ok = engine.step_backward();
    REQUIRE(ok);
    REQUIRE(g.getNode(NodeId(1))->get().assignment == Assignment::Unassigned);
    REQUIRE(g.getNode(NodeId(1))->get().activity == Catch::Approx(0.0));
}

TEST_CASE("Backward evolution reverts clause addition", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ClauseEvent{ClauseAction::Add, {1, 3}});
    REQUIRE(g.edgeCount() == 1);

    engine.step_backward();
    REQUIRE(g.edgeCount() == 0);
}

TEST_CASE("Backward evolution reverts clause removal", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ClauseEvent{ClauseAction::Add, {1, 3}});
    engine.process_event(ClauseEvent{ClauseAction::Remove, {1, 3}});
    REQUIRE(g.edgeCount() == 0);

    engine.step_backward();
    REQUIRE(g.edgeCount() == 1);
}

TEST_CASE("Backward evolution tracks conflict count", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ConflictEvent{1});
    engine.process_event(ConflictEvent{2});
    REQUIRE(engine.current_conflict() == 2);

    engine.step_backward();
    REQUIRE(engine.current_conflict() == 1);

    engine.step_backward();
    REQUIRE(engine.current_conflict() == 0);
}

TEST_CASE("Backward evolution returns false when empty", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    REQUIRE_FALSE(engine.step_backward());
}

TEST_CASE("History depth matches event count", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});
    engine.process_event(VariableAssignment{NodeId(2), 1, 0.3});
    engine.process_event(ConflictEvent{1});

    REQUIRE(engine.history_depth() == 3);
    REQUIRE(engine.event_count() == 3);
}

TEST_CASE("Conflict scanning jumps backward", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    for (int i = 1; i <= 10; ++i) {
        engine.process_event(ConflictEvent{i});
        engine.process_event(VariableAssignment{NodeId(i % 5 + 1), 1, 0.1 * i});
    }
    REQUIRE(engine.current_conflict() == 10);

    bool ok = engine.jump_to_conflict(5);
    REQUIRE(ok);
    REQUIRE(engine.current_conflict() == 5);
}

TEST_CASE("Conflict scanning rejects out-of-range forward", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(ConflictEvent{1});
    engine.process_event(ConflictEvent{2});

    REQUIRE_FALSE(engine.jump_to_conflict(99));
}

TEST_CASE("Observer receives node_assigned callback", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    TestObserver obs;
    engine.add_observer(&obs);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});

    REQUIRE(obs.node_assigned_calls.size() == 1);
    REQUIRE(obs.node_assigned_calls[0].var == NodeId(1));
    REQUIRE(obs.node_assigned_calls[0].state == 1);
    REQUIRE(obs.node_assigned_calls[0].activity == Catch::Approx(0.5));
}

TEST_CASE("Observer receives clause callbacks", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    TestObserver obs;
    engine.add_observer(&obs);

    engine.process_event(ClauseEvent{ClauseAction::Add, {1, 2, 3}});
    REQUIRE(obs.clause_added_calls.size() == 1);
    REQUIRE(obs.clause_added_calls[0].literals == std::vector<int>{1, 2, 3});

    engine.process_event(ClauseEvent{ClauseAction::Remove, {1, 2}});
    REQUIRE(obs.clause_removed_calls.size() == 1);
}

TEST_CASE("Observer receives conflict callback", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    TestObserver obs;
    engine.add_observer(&obs);

    engine.process_event(ConflictEvent{7});
    REQUIRE(obs.conflict_calls.size() == 1);
    REQUIRE(obs.conflict_calls[0] == 7);
}

TEST_CASE("Multiple observers receive callbacks", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    TestObserver obs1, obs2;
    engine.add_observer(&obs1);
    engine.add_observer(&obs2);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});

    REQUIRE(obs1.node_assigned_calls.size() == 1);
    REQUIRE(obs2.node_assigned_calls.size() == 1);
}

TEST_CASE("Remove observer stops callbacks", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);
    TestObserver obs;
    engine.add_observer(&obs);
    engine.remove_observer(&obs);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});
    REQUIRE(obs.node_assigned_calls.empty());
}

TEST_CASE("process_line parses and applies pipe events", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_line("v d 1 0.75 1");
    REQUIRE(g.getNode(NodeId(1))->get().assignment == Assignment::True);

    engine.process_line("c + 1 2 0");
    REQUIRE(g.edgeCount() > 0);

    engine.process_line("! 1");
    REQUIRE(engine.current_conflict() == 1);
}

TEST_CASE("VIG mode creates unsigned variable edges", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g, GraphMode::VIG);

    engine.process_event(ClauseEvent{ClauseAction::Add, {3, -5}});
    bool found = false;
    for (const auto& [eid, edge] : g.getEdges()) {
        if ((edge.source == NodeId(3) && edge.target == NodeId(5)) ||
            (edge.source == NodeId(5) && edge.target == NodeId(3))) {
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("LIG mode creates signed literal edges", "[evolution]") {
    auto g = make_test_graph(10);
    EvolutionEngine engine(g, GraphMode::LIG);

    engine.process_event(ClauseEvent{ClauseAction::Add, {3, -5}});

    auto pos3 = engine.literal_to_node_id(3, GraphMode::LIG);
    auto neg5 = engine.literal_to_node_id(-5, GraphMode::LIG);

    REQUIRE(pos3 != neg5);

    bool found = false;
    for (const auto& [eid, edge] : g.getEdges()) {
        if ((edge.source == pos3 && edge.target == neg5) ||
            (edge.source == neg5 && edge.target == pos3)) {
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("clear_history resets engine state", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    engine.process_event(VariableAssignment{NodeId(1), 1, 0.5});
    engine.process_event(ConflictEvent{1});
    REQUIRE(engine.history_depth() == 2);

    engine.clear_history();
    REQUIRE(engine.history_depth() == 0);
    REQUIRE(engine.event_count() == 0);
    REQUIRE(engine.current_conflict() == 0);
}

TEST_CASE("File buffering loads lines asynchronously", "[evolution]") {
    auto g = make_test_graph(5);
    EvolutionEngine engine(g);

    std::string tmpfile = "/tmp/satgraf-test-buffer-" + std::to_string(::getpid()) + ".txt";
    {
        std::ofstream f(tmpfile);
        f << "v d 1 0.5 1\n";
        f << "c + 1 2 0\n";
        f << "! 1\n";
    }

    engine.buffer_file(tmpfile);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    REQUIRE(engine.has_buffered_file());
    auto lines = engine.take_buffered_file();
    REQUIRE(lines.has_value());
    REQUIRE(lines->size() == 3);
    REQUIRE((*lines)[0] == "v d 1 0.5 1");
    REQUIRE((*lines)[2] == "! 1");

    std::filesystem::remove(tmpfile);
}
