#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/clause.hpp"
#include "satgraf/types.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace satgraf::evolution {

// ---------------------------------------------------------------------------
// 8.1 — Event types
// ---------------------------------------------------------------------------

struct VariableAssignment {
    graph::NodeId var;
    int state;        // 0=unassigned, 1=true, -1=false (maps from pipe: 0/1)
    double activity;
};

enum class ClauseAction { Add, Remove };

struct ClauseEvent {
    ClauseAction action;
    std::vector<int> literals;  // signed: positive=var, negative=negated
};

struct ConflictEvent {
    int conflict_num;
};

using SolverEvent = std::variant<VariableAssignment, ClauseEvent, ConflictEvent>;

// ---------------------------------------------------------------------------
// 8.2 — Event stream parser
// ---------------------------------------------------------------------------

inline std::optional<SolverEvent> parse_event_line(const std::string& line) {
    if (line.empty()) return std::nullopt;

    if (line[0] == 'v') {
        // v <id> <state> <activity>
        // state: 0=unassigned, 1=true (pipe protocol uses d|p for decision/propagation)
        // Original format: "v d|p state activity id"
        // Simplified format: "v id state activity"
        std::istringstream iss(line.substr(1));
        int id, state_int;
        double activity;
        if (!(iss >> id >> state_int >> activity)) return std::nullopt;
        VariableAssignment va;
        va.var = graph::NodeId(static_cast<uint32_t>(id));
        va.state = state_int;
        va.activity = activity;
        return va;
    }

    if (line[0] == 'c' && line.size() > 2 && (line[1] == ' ')) {
        // c +|- literals... 0
        std::istringstream iss(line.substr(2));
        char sign;
        if (!(iss >> sign)) return std::nullopt;

        ClauseEvent ce;
        ce.action = (sign == '+') ? ClauseAction::Add : ClauseAction::Remove;

        int lit;
        while (iss >> lit) {
            if (lit == 0) break;
            ce.literals.push_back(lit);
        }
        if (ce.literals.empty()) return std::nullopt;
        return ce;
    }

    if (line[0] == '!') {
        // ! conflict_num
        std::istringstream iss(line.substr(1));
        int conflict_num;
        if (!(iss >> conflict_num)) return std::nullopt;
        return ConflictEvent{conflict_num};
    }

    return std::nullopt;
}

// Extended parser for the original satGraf pipe format:
// "v d|p state activity id"
inline std::optional<SolverEvent> parse_pipe_event(const std::string& line) {
    if (line.empty()) return std::nullopt;

    if (line.size() >= 2 && line[0] == 'v' && line[1] == ' ') {
        std::istringstream iss(line.substr(2));
        char type;  // 'd' (decision) or 'p' (propagation)
        int state_int, id;
        double activity;

        if (!(iss >> type >> state_int >> activity >> id)) return std::nullopt;

        VariableAssignment va;
        va.var = graph::NodeId(static_cast<uint32_t>(id));
        va.state = state_int;
        va.activity = activity;
        return va;
    }

    if (line.size() >= 3 && line[0] == 'c' && line[1] == ' ') {
        std::istringstream iss(line.substr(2));
        char sign;
        if (!(iss >> sign)) return std::nullopt;

        ClauseEvent ce;
        ce.action = (sign == '+') ? ClauseAction::Add : ClauseAction::Remove;

        int lit;
        while (iss >> lit) {
            if (lit == 0) break;
            ce.literals.push_back(lit);
        }
        if (ce.literals.empty()) return std::nullopt;
        return ce;
    }

    if (line[0] == '!') {
        std::istringstream iss(line.substr(1));
        int conflict_num;
        if (!(iss >> conflict_num)) return std::nullopt;
        return ConflictEvent{conflict_num};
    }

    return std::nullopt;
}

// ---------------------------------------------------------------------------
// 8.8 — Observer pattern
// ---------------------------------------------------------------------------

class EvolutionObserver {
public:
    virtual ~EvolutionObserver() = default;
    virtual void node_assigned(graph::NodeId var, int state, double activity) {
        (void)var; (void)state; (void)activity;
    }
    virtual void clause_added(const std::vector<int>& literals) {
        (void)literals;
    }
    virtual void clause_removed(const std::vector<int>& literals) {
        (void)literals;
    }
    virtual void conflict(int conflict_num) {
        (void)conflict_num;
    }
    virtual void update_graph() {}
    virtual void new_file_ready(const std::string& filename) {
        (void)filename;
    }
    virtual void decision_variable(graph::NodeId var) {
        (void)var;
    }
};

// ---------------------------------------------------------------------------
// 8.3/8.4 — State snapshot for backward evolution
// ---------------------------------------------------------------------------

struct NodeSnapshot {
    graph::NodeId id;
    graph::Assignment prev_assignment;
    double prev_activity;
};

struct ClauseSnapshot {
    std::vector<int> literals;
    bool was_added;  // true = forward step added it, backward removes it
};

struct HistoryEntry {
    std::vector<NodeSnapshot> node_changes;
    std::optional<ClauseSnapshot> clause_change;
    std::optional<int> conflict_num;       // if this step was a conflict
    std::optional<graph::NodeId> decision_var;  // if this step set a decision variable
};

// ---------------------------------------------------------------------------
// 8.9 — Graph mode (VIG vs LIG)
// ---------------------------------------------------------------------------

enum class GraphMode { VIG, LIG };

// ---------------------------------------------------------------------------
// Evolution Engine
// ---------------------------------------------------------------------------

class EvolutionEngine {
public:
    using Graph = graph::Graph<graph::Node, graph::Edge>;

    explicit EvolutionEngine(Graph& graph, GraphMode mode = GraphMode::VIG)
        : graph_(graph), mode_(mode) {}

    ~EvolutionEngine() {
        if (buffer_thread_.joinable()) {
            buffer_thread_.join();
        }
    }

    EvolutionEngine(const EvolutionEngine&) = delete;
    EvolutionEngine& operator=(const EvolutionEngine&) = delete;

    // --- 8.3 Forward evolution ---

    void process_event(const SolverEvent& event) {
        HistoryEntry entry;

        std::visit([this, &entry](const auto& e) {
            this->apply_forward(e, entry);
        }, event);

        if (entry.node_changes.empty() && !entry.clause_change.has_value()
            && !entry.conflict_num.has_value()) {
            return;
        }

        history_.push_back(std::move(entry));
        event_count_++;
    }

    void process_line(const std::string& line) {
        auto ev = parse_pipe_event(line);
        if (ev) process_event(*ev);
    }

    // --- 8.4 Backward evolution ---

    bool step_backward() {
        if (history_.empty()) return false;

        HistoryEntry entry = std::move(history_.back());
        history_.pop_back();

        // Undo node changes in reverse
        for (auto it = entry.node_changes.rbegin(); it != entry.node_changes.rend(); ++it) {
            auto node_opt = graph_.getNode(it->id);
            if (node_opt) {
                auto& node = node_opt->get();
                node.assignment = it->prev_assignment;
                node.activity = it->prev_activity;
            }
        }

        // Undo clause change
        if (entry.clause_change) {
            const auto& cs = *entry.clause_change;
            if (cs.was_added) {
                remove_clause_edges(cs.literals);
            } else {
                add_clause_edges(cs.literals);
            }
        }

        // Undo conflict tracking
        if (entry.conflict_num) {
            current_conflict_--;
            notify_obsolvers_conflict_undone();
        }

        // Undo decision variable
        if (entry.decision_var) {
            if (decision_var_ == *entry.decision_var) {
                decision_var_ = std::nullopt;
            }
        }

        event_count_--;
        return true;
    }

    // --- 8.6 Conflict scanning ---

    bool jump_to_conflict(int target_conflict) {
        if (target_conflict < 0) return false;

        if (target_conflict < current_conflict_) {
            // Need to go backward
            while (current_conflict_ > target_conflict && !history_.empty()) {
                const auto& entry = history_.back();
                if (entry.conflict_num && *entry.conflict_num <= target_conflict) {
                    break;
                }
                step_backward();
            }
            return current_conflict_ == target_conflict;
        }

        if (target_conflict > current_conflict_) {
            return false;  // Can't jump forward without new events
        }

        return true;  // Already there
    }

    // --- 8.7 Decision variable tracking ---

    std::optional<graph::NodeId> decision_variable() const {
        return decision_var_;
    }

    // --- 8.8 Observer management ---

    void add_observer(EvolutionObserver* observer) {
        auto it = std::find(observers_.begin(), observers_.end(), observer);
        if (it == observers_.end()) {
            observers_.push_back(observer);
        }
    }

    void remove_observer(EvolutionObserver* observer) {
        auto it = std::find(observers_.begin(), observers_.end(), observer);
        if (it != observers_.end()) {
            observers_.erase(it);
        }
    }

    // --- 8.5 File buffering ---

    void buffer_file(const std::string& path) {
        if (buffer_thread_.joinable()) {
            buffer_thread_.join();
        }
        buffer_ready_ = false;
        buffer_thread_ = std::thread([this, path]() {
            BufferedFile bf;
            bf.path = path;
            std::ifstream file(path);
            if (!file.is_open()) {
                return;
            }
            std::string line;
            while (std::getline(file, line)) {
                bf.lines.push_back(std::move(line));
            }
            {
                std::lock_guard<std::mutex> lock(buffer_mutex_);
                next_buffer_ = std::move(bf);
                buffer_ready_ = true;
            }
            buffer_cv_.notify_one();
            for (auto* obs : observers_) {
                obs->new_file_ready(path);
            }
        });
    }

    bool has_buffered_file() const {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        return buffer_ready_;
    }

    std::optional<std::vector<std::string>> take_buffered_file() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (!buffer_ready_ || !next_buffer_.has_value()) {
            return std::nullopt;
        }
        buffer_ready_ = false;
        auto lines = std::move(next_buffer_->lines);
        next_buffer_.reset();
        return lines;
    }

    // --- Accessors ---

    int current_conflict() const { return current_conflict_; }
    size_t history_depth() const { return history_.size(); }
    size_t event_count() const { return event_count_; }
    GraphMode mode() const { return mode_; }

    void set_mode(GraphMode mode) {
        mode_ = mode;
    }

    void clear_history() {
        history_.clear();
        event_count_ = 0;
        current_conflict_ = 0;
        decision_var_ = std::nullopt;
    }

    // --- 8.9 Mode helpers ---

    graph::NodeId literal_to_node_id(int literal, GraphMode m) const {
        if (m == GraphMode::VIG) {
            return graph::NodeId(static_cast<uint32_t>(std::abs(literal)));
        }
        // LIG: encode sign into node id. Positive literal N → 2*N, negative → 2*N+1
        if (literal > 0) {
            return graph::NodeId(static_cast<uint32_t>(literal * 2));
        }
        return graph::NodeId(static_cast<uint32_t>((-literal) * 2 + 1));
    }

private:
    void apply_forward(const VariableAssignment& va, HistoryEntry& entry) {
        auto node_opt = graph_.getNode(va.var);
        if (!node_opt) {
            // Create node if it doesn't exist (evolution may introduce new vars)
            graph_.createNode(va.var, std::to_string(va.var.value));
            node_opt = graph_.getNode(va.var);
        }
        auto& node = node_opt->get();

        NodeSnapshot snap;
        snap.id = va.var;
        snap.prev_assignment = node.assignment;
        snap.prev_activity = node.activity;
        entry.node_changes.push_back(snap);

        if (va.state == 0) {
            node.assignment = graph::Assignment::Unassigned;
        } else if (va.state > 0) {
            node.assignment = graph::Assignment::True;
        } else {
            node.assignment = graph::Assignment::False;
        }
        node.activity = va.activity;

        for (auto* obs : observers_) {
            obs->node_assigned(va.var, va.state, va.activity);
        }
    }

    void apply_forward(const ClauseEvent& ce, HistoryEntry& entry) {
        entry.clause_change = ClauseSnapshot{
            ce.literals,
            ce.action == ClauseAction::Add
        };

        if (ce.action == ClauseAction::Add) {
            add_clause_edges(ce.literals);
            for (auto* obs : observers_) {
                obs->clause_added(ce.literals);
            }
        } else {
            remove_clause_edges(ce.literals);
            for (auto* obs : observers_) {
                obs->clause_removed(ce.literals);
            }
        }
    }

    void apply_forward(const ConflictEvent& ce, HistoryEntry& entry) {
        entry.conflict_num = ce.conflict_num;
        current_conflict_ = ce.conflict_num;

        // 8.7: Clear decision variable on conflict
        if (decision_var_.has_value()) {
            decision_var_ = std::nullopt;
        }

        for (auto* obs : observers_) {
            obs->conflict(ce.conflict_num);
        }
    }

    void add_clause_edges(const std::vector<int>& literals) {
        for (size_t i = 0; i < literals.size(); ++i) {
            for (size_t j = i + 1; j < literals.size(); ++j) {
                auto src = literal_to_node_id(literals[i], mode_);
                auto tgt = literal_to_node_id(literals[j], mode_);

                // Ensure nodes exist
                if (!graph_.getNode(src)) {
                    graph_.createNode(src, std::to_string(src.value));
                }
                if (!graph_.getNode(tgt)) {
                    graph_.createNode(tgt, std::to_string(tgt.value));
                }

                graph_.createEdge(src, tgt);
            }
        }
    }

    void remove_clause_edges(const std::vector<int>& literals) {
        for (size_t i = 0; i < literals.size(); ++i) {
            for (size_t j = i + 1; j < literals.size(); ++j) {
                auto src = literal_to_node_id(literals[i], mode_);
                auto tgt = literal_to_node_id(literals[j], mode_);

                // Find and remove edge(s) between src and tgt
                auto& edges = graph_.getEdges();
                std::vector<graph::EdgeId> to_remove;
                for (const auto& [eid, edge] : edges) {
                    if ((edge.source == src && edge.target == tgt) ||
                        (edge.source == tgt && edge.target == src)) {
                        to_remove.push_back(eid);
                    }
                }
                for (auto eid : to_remove) {
                    graph_.removeEdge(eid);
                }
            }
        }
    }

    void notify_obsolvers_conflict_undone() {
        for (auto* obs : observers_) {
            obs->update_graph();
        }
    }

    struct BufferedFile {
        std::string path;
        std::vector<std::string> lines;
    };

    Graph& graph_;
    GraphMode mode_;

    std::vector<HistoryEntry> history_;
    std::vector<EvolutionObserver*> observers_;

    int current_conflict_{0};
    size_t event_count_{0};
    std::optional<graph::NodeId> decision_var_;

    // 8.5 File buffering
    std::thread buffer_thread_;
    mutable std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    std::optional<BufferedFile> next_buffer_;
    bool buffer_ready_{false};
};

}  // namespace satgraf::evolution
