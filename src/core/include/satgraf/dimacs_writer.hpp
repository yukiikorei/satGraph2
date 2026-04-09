#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/clause.hpp"
#include "satgraf/node.hpp"
#include "satgraf/types.hpp"

#include <algorithm>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace satgraf::graph {

template<typename NodeT, typename EdgeT>
class DimacsWriter {
public:
    static void write(std::ostream& out, const Graph<NodeT, EdgeT>& g) {
        const auto& nodes = g.getNodes();
        const auto& clauses = g.getClauses();

        out << "p cnf " << nodes.size() << " " << clauses.size() << '\n';

        std::map<uint32_t, std::string> sorted_names;
        for (const auto& [nid, node] : nodes) {
            sorted_names[static_cast<uint32_t>(nid)] = node.name;
        }
        for (const auto& [val, name] : sorted_names) {
            out << "c " << val << " " << name << '\n';
        }

        for (const auto& clause : clauses) {
            std::vector<int> literals;
            literals.reserve(clause.size());
            for (const auto& [nid, polarity] : clause) {
                int lit = static_cast<int>(static_cast<uint32_t>(nid));
                if (!polarity) lit = -lit;
                literals.push_back(lit);
            }
            std::sort(literals.begin(), literals.end(),
                      [](int a, int b) { return std::abs(a) < std::abs(b); });
            for (int lit : literals) {
                out << lit << ' ';
            }
            out << "0\n";
        }
    }
};

} // namespace satgraf::graph
