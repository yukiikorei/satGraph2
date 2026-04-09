#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"
#include "satgraf/clause.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace satgraf::dimacs {

/// Parsing mode for DIMACS CNF files.
enum class Mode {
    /// Variable Interaction Graph: nodes represent variables (|literal|),
    /// edges connect variables that co-occur in the same clause.
    VIG,

    /// Literal Interaction Graph: nodes represent signed literals,
    /// edges connect literals that co-occur in the same clause.
    ///
    /// LIG node ID scheme:
    ///   Positive literal +X  ->  NodeId(2 * X)       name: "+<varname>"
    ///   Negative literal -X  ->  NodeId(2 * X + 1)   name: "-<varname>"
    ///
    /// <varname> is the variable name from a "c <id> <name>" line,
    /// or the variable number as a string if no name was provided.
    LIG
};

/// Progress callback type. Called with a value in [0.0, 1.0].
using ProgressCallback = std::function<void(double)>;

/// Parses DIMACS CNF files into a graph representation.
///
/// Supports both VIG (Variable Interaction Graph) and LIG (Literal Interaction
/// Graph) modes. Variable names can be provided via "c <id> <name>" lines.
/// Optional regex patterns enable automatic variable grouping.
class Parser {
public:
    Parser() = default;

    /// Construct a parser with optional grouping patterns and progress callback.
    /// @param group_patterns Regex patterns for variable grouping. First match wins;
    ///                       captured subgroup (or full match) becomes the group label.
    ///                       Unmatched variables receive "ungrouped".
    /// @param callback Progress callback receiving values in [0.0, 1.0].
    explicit Parser(std::vector<std::regex> group_patterns,
                    ProgressCallback callback = nullptr)
        : group_patterns_(std::move(group_patterns))
        , callback_(std::move(callback)) {}

    /// Parse a DIMACS CNF file and return the constructed graph.
    /// @param file_path Path to the .cnf file.
    /// @param mode VIG or LIG parsing mode.
    /// @return The constructed graph with nodes, edges, and clauses.
    /// @throws std::runtime_error on file I/O errors or invalid DIMACS format.
    graph::Graph<graph::Node, graph::Edge> parse(const std::string& file_path,
                                                  Mode mode) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + file_path);
        }

        const auto file_size = std::filesystem::file_size(file_path);

        graph::Graph<graph::Node, graph::Edge> result;

        // Edge deduplication: canonical (min, max) pair -> EdgeId
        std::map<std::pair<uint32_t, uint32_t>, graph::EdgeId> edge_index;

        // Variable names from c-lines: variable_id -> name
        std::unordered_map<uint32_t, std::string> var_names;

        bool p_line_seen = false;
        std::vector<int> current_literals;
        std::size_t line_number = 0;
        std::string line;

        const auto report_progress = [&](std::streampos pos) {
            if (callback_ && file_size > 0) {
                const double progress =
                    static_cast<double>(pos) / static_cast<double>(file_size);
                callback_(std::min(progress, 1.0));
            }
        };

        const auto get_var_name = [&](uint32_t var) -> std::string {
            const auto it = var_names.find(var);
            if (it != var_names.end()) return it->second;
            return std::to_string(var);
        };

        const auto ensure_node = [&](uint32_t id, const std::string& name) {
            if (!result.getNode(graph::NodeId(id)).has_value()) {
                result.createNode(graph::NodeId(id), name);
            }
        };

        const auto get_or_create_edge = [&](uint32_t a, uint32_t b) {
            const auto key = std::minmax(a, b);
            const auto it = edge_index.find(key);
            if (it != edge_index.end()) {
                result.getEdges().at(it->second).weight += 1.0;
            } else {
                auto& edge = result.createEdge(
                    graph::NodeId(key.first), graph::NodeId(key.second));
                edge_index.emplace(key, edge.id);
                result.connect(edge.id, graph::NodeId(key.first));
            }
        };

        const auto flush_clause_vig = [&]() {
            if (current_literals.empty()) {
                result.addClause();
                return;
            }

            std::set<uint32_t> vars;
            auto& clause = result.addClause();

            for (const int lit : current_literals) {
                const uint32_t var = static_cast<uint32_t>(std::abs(lit));
                const bool polarity = (lit > 0);
                vars.insert(var);
                clause.add_literal(graph::NodeId(var), polarity);
                ensure_node(var, get_var_name(var));
            }

            std::vector<uint32_t> var_vec(vars.begin(), vars.end());
            for (std::size_t i = 0; i < var_vec.size(); ++i) {
                for (std::size_t j = i + 1; j < var_vec.size(); ++j) {
                    get_or_create_edge(var_vec[i], var_vec[j]);
                }
            }
        };

        const auto flush_clause_lig = [&]() {
            if (current_literals.empty()) {
                result.addClause();
                return;
            }

            auto& clause = result.addClause();
            std::set<uint32_t> lit_nodes;

            for (const int lit : current_literals) {
                const uint32_t var = static_cast<uint32_t>(std::abs(lit));
                const bool positive = (lit > 0);
                const uint32_t node_id = positive ? 2 * var : 2 * var + 1;
                const std::string name =
                    (positive ? std::string("+") : std::string("-")) +
                    get_var_name(var);

                lit_nodes.insert(node_id);
                ensure_node(node_id, name);
                clause.add_literal(graph::NodeId(node_id), true);
            }

            std::vector<uint32_t> node_vec(lit_nodes.begin(), lit_nodes.end());
            for (std::size_t i = 0; i < node_vec.size(); ++i) {
                for (std::size_t j = i + 1; j < node_vec.size(); ++j) {
                    get_or_create_edge(node_vec[i], node_vec[j]);
                }
            }
        };

        const auto flush_clause = [&]() {
            if (mode == Mode::VIG) {
                flush_clause_vig();
            } else {
                flush_clause_lig();
            }
            current_literals.clear();
        };

        while (std::getline(file, line)) {
            ++line_number;
            report_progress(file.tellg());

            const auto start = line.find_first_not_of(" \t\r");
            if (start == std::string::npos) continue;

            const char first = line[start];

            // --- Comment or variable-naming line ---
            if (first == 'c') {
                std::istringstream iss(line.substr(start));
                std::string c_tag;
                int id = 0;
                std::string name;

                if (iss >> c_tag >> id >> name) {
                    if (id > 0) {
                        const auto var = static_cast<uint32_t>(id);
                        var_names[var] = name;

                        if (mode == Mode::VIG) {
                            auto opt = result.getNode(graph::NodeId(var));
                            if (opt.has_value()) {
                                opt->get().name = name;
                            }
                        } else {
                            const uint32_t pos_id = 2 * var;
                            const uint32_t neg_id = 2 * var + 1;

                            auto opt_pos = result.getNode(graph::NodeId(pos_id));
                            if (opt_pos.has_value()) {
                                opt_pos->get().name = "+" + name;
                            }
                            auto opt_neg = result.getNode(graph::NodeId(neg_id));
                            if (opt_neg.has_value()) {
                                opt_neg->get().name = "-" + name;
                            }
                        }
                    }
                }
                // Lines that don't parse as "c <id> <name>" are plain comments.
                continue;
            }

            // --- Problem line ---
            if (first == 'p') {
                if (p_line_seen) {
                    throw std::runtime_error(
                        "Duplicate p-line at line " +
                        std::to_string(line_number) + " in " + file_path);
                }

                std::istringstream iss(line.substr(start));
                std::string p_tag;
                std::string format;
                if (!(iss >> p_tag >> format)) {
                    throw std::runtime_error(
                        "Invalid p-line format at line " +
                        std::to_string(line_number) + " in " + file_path);
                }

                uint32_t nv = 0;
                uint32_t nc = 0;
                if (!(iss >> nv >> nc)) {
                    throw std::runtime_error(
                        "Invalid p-line format at line " +
                        std::to_string(line_number) + " in " + file_path);
                }
                // nv and nc are informational; the graph reflects actual data.
                (void)nv;
                (void)nc;

                p_line_seen = true;
                continue;
            }

            // --- Clause data ---
            if (!p_line_seen) {
                throw std::runtime_error(
                    "Clause data before p-line at line " +
                    std::to_string(line_number) + " in " + file_path);
            }

            {
                std::istringstream iss(line.substr(start));
                std::string token;
                while (iss >> token) {
                    int lit = 0;
                    try {
                        std::size_t pos = 0;
                        lit = std::stoi(token, &pos);
                        if (pos != token.size()) {
                            throw std::invalid_argument("trailing characters");
                        }
                    } catch (const std::invalid_argument&) {
                        throw std::runtime_error(
                            "Invalid literal '" + token + "' at line " +
                            std::to_string(line_number) + " in " + file_path);
                    } catch (const std::out_of_range&) {
                        throw std::runtime_error(
                            "Literal out of range '" + token + "' at line " +
                            std::to_string(line_number) + " in " + file_path);
                    }

                    if (lit == 0) {
                        flush_clause();
                    } else {
                        current_literals.push_back(lit);
                    }
                }
            }
        }

        // Unterminated clause at EOF: treat as a complete clause.
        if (!current_literals.empty()) {
            flush_clause();
        }

        // Apply regex-based grouping to all nodes.
        apply_all_grouping(result, mode);

        // Final progress report.
        if (callback_) {
            callback_(1.0);
        }

        return result;
    }

private:
    std::vector<std::regex> group_patterns_;
    ProgressCallback callback_;

    void apply_all_grouping(
        graph::Graph<graph::Node, graph::Edge>& graph,
        Mode mode) const {
        if (group_patterns_.empty()) return;

        for (auto& [nid, node] : graph.getNodes()) {
            const uint32_t id = static_cast<uint32_t>(nid);
            std::string var_name;

            if (mode == Mode::VIG) {
                var_name = node.name.empty() ? std::to_string(id)
                                             : node.name;
            } else {
                // LIG node names are "+<name>" or "-<name>".
                // Strip the sign prefix to get the underlying variable name.
                var_name = (node.name.size() > 1)
                               ? node.name.substr(1)
                               : std::to_string(id / 2);
            }

            apply_regex_group(node, var_name);
        }
    }

    void apply_regex_group(graph::Node& node,
                           const std::string& var_name) const {
        for (const auto& pattern : group_patterns_) {
            std::smatch match;
            if (std::regex_search(var_name, match, pattern)) {
                if (match.size() > 1 && match[1].matched) {
                    node.groups.push_back(match[1].str());
                } else {
                    node.groups.push_back(match[0].str());
                }
                return;
            }
        }
        node.groups.push_back("ungrouped");
    }
};

} // namespace satgraf::dimacs
