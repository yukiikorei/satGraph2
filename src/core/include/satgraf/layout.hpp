#pragma once

#include "satgraf/graph.hpp"
#include "satgraf/node.hpp"
#include "satgraf/edge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <set>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

namespace satgraf::layout {

struct Coordinate {
    double x{0.0};
    double y{0.0};
};

struct Coordinate3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};
using Coordinate3DMap = std::unordered_map<graph::NodeId, Coordinate3D>;

using CoordinateMap = std::unordered_map<graph::NodeId, Coordinate>;
using ProgressCallback = std::function<void(double)>;

class Layout {
public:
    virtual ~Layout() = default;

    virtual CoordinateMap compute(
        const graph::Graph<graph::Node, graph::Edge>& graph,
        ProgressCallback progress_callback = nullptr) = 0;
};

class FruchtermanReingoldLayout : public Layout {
public:
    explicit FruchtermanReingoldLayout(std::size_t iterations = 500,
                                       double width = 1024.0,
                                       double height = 1024.0,
                                       double margin = 0.0,
                                       double k_scale = 0.46)
        : iterations_(iterations)
        , width_(width)
        , height_(height)
        , margin_(margin)
        , k_scale_(k_scale) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override {
        CoordinateMap coordinates;
        const auto& nodes = input_graph.getNodes();
        if (nodes.empty()) {
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(nodes.size());
        for (const auto& [node_id, node] : nodes) {
            (void)node;
            node_ids.push_back(node_id);
        }
        std::sort(node_ids.begin(), node_ids.end());

        if (progress_callback) {
            progress_callback(0.0);
        }

        if (node_ids.size() == 1) {
            coordinates.emplace(node_ids.front(), Coordinate{width_ * 0.5, height_ * 0.5});
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::vector<Coordinate> positions(node_ids.size());
        initialize_positions(positions);

        const double area = width_ * height_;
        const double node_count = static_cast<double>(node_ids.size());
        const double k = k_scale_ * std::sqrt(area / node_count);
        const double initial_temperature = std::max(width_, height_) * 0.1;

        const auto& edges = input_graph.getEdges();
        std::unordered_map<graph::NodeId, std::size_t> node_index;
        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            node_index.emplace(node_ids[i], i);
        }

        const std::size_t effective_iterations = std::max<std::size_t>(iterations_, 1);
        for (std::size_t iteration = 0; iteration < effective_iterations; ++iteration) {
            std::vector<Coordinate> displacements(node_ids.size(), Coordinate{});

            for (std::size_t i = 0; i < positions.size(); ++i) {
                for (std::size_t j = i + 1; j < positions.size(); ++j) {
                    const double dx = positions[i].x - positions[j].x;
                    const double dy = positions[i].y - positions[j].y;
                    const double distance = std::max(euclidean_distance(dx, dy), min_distance());
                    const double force = (k * k) / distance;
                    const double fx = (dx / distance) * force;
                    const double fy = (dy / distance) * force;

                    displacements[i].x += fx;
                    displacements[i].y += fy;
                    displacements[j].x -= fx;
                    displacements[j].y -= fy;
                }
            }

            for (const auto& [edge_id, edge] : edges) {
                (void)edge_id;
                const auto src_it = node_index.find(edge.source);
                const auto tgt_it = node_index.find(edge.target);
                if (src_it == node_index.end() || tgt_it == node_index.end()) {
                    continue;
                }

                const std::size_t src = src_it->second;
                const std::size_t tgt = tgt_it->second;
                const double dx = positions[src].x - positions[tgt].x;
                const double dy = positions[src].y - positions[tgt].y;
                const double distance = std::max(euclidean_distance(dx, dy), min_distance());
                const double force = (distance * distance) / k;
                const double fx = (dx / distance) * force;
                const double fy = (dy / distance) * force;

                displacements[src].x -= fx;
                displacements[src].y -= fy;
                displacements[tgt].x += fx;
                displacements[tgt].y += fy;
            }

            const double progress = static_cast<double>(iteration + 1) /
                                    static_cast<double>(effective_iterations);
            const double temperature = std::max(initial_temperature * (1.0 - progress), 0.0);

            for (std::size_t i = 0; i < positions.size(); ++i) {
                const double displacement_norm =
                    std::max(euclidean_distance(displacements[i].x, displacements[i].y),
                             min_distance());
                const double limited = std::min(displacement_norm, temperature);
                positions[i].x += (displacements[i].x / displacement_norm) * limited;
                positions[i].y += (displacements[i].y / displacement_norm) * limited;
            }

            if (progress_callback) {
                progress_callback(progress);
            }
        }

        rescale_positions(positions);

        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            coordinates.emplace(node_ids[i], positions[i]);
        }

        if (progress_callback) {
            progress_callback(1.0);
        }

        return coordinates;
    }

private:
    static double min_distance() noexcept { return 1e-9; }

    static double euclidean_distance(double dx, double dy) noexcept {
        return std::sqrt(dx * dx + dy * dy);
    }

    void initialize_positions(std::vector<Coordinate>& positions) const {
        const double center_x = width_ * 0.5;
        const double center_y = height_ * 0.5;
        const double radius = std::max(std::min(width_, height_) * 0.35, 1.0);
        const double tau = 6.28318530717958647692;

        for (std::size_t i = 0; i < positions.size(); ++i) {
            const double angle = tau * static_cast<double>(i) /
                                 static_cast<double>(positions.size());
            positions[i] = Coordinate{center_x + radius * std::cos(angle),
                                      center_y + radius * std::sin(angle)};
        }
    }

    void rescale_positions(std::vector<Coordinate>& positions) const {
        const double target_min_x = margin_;
        const double target_max_x = std::max(margin_, width_ - margin_);
        const double target_min_y = margin_;
        const double target_max_y = std::max(margin_, height_ - margin_);

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();

        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }

        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double target_span_x = target_max_x - target_min_x;
        const double target_span_y = target_max_y - target_min_y;

        for (auto& position : positions) {
            position.x = (span_x <= min_distance())
                ? (target_min_x + target_max_x) * 0.5
                : target_min_x + ((position.x - min_x) / span_x) * target_span_x;
            position.y = (span_y <= min_distance())
                ? (target_min_y + target_max_y) * 0.5
                : target_min_y + ((position.y - min_y) / span_y) * target_span_y;
        }
    }

    std::size_t iterations_;
    double width_;
    double height_;
    double margin_;
    double k_scale_;
};

class ForceAtlas2Layout : public Layout {
public:
    explicit ForceAtlas2Layout(std::size_t iterations = 200,
                               double width = 1024.0,
                               double height = 1024.0,
                               double margin = 0.0,
                               double scaling_ratio = 10.0,
                               double gravity = 1.0,
                               bool lin_log_mode = false,
                               double edge_weight_influence = 1.0,
                               bool barnes_hut_optimize = true,
                               double barnes_hut_theta = 1.2,
                               std::size_t thread_count = default_thread_count())
        : iterations_(iterations)
        , width_(width)
        , height_(height)
        , margin_(margin)
        , scaling_ratio_(scaling_ratio)
        , gravity_(gravity)
        , lin_log_mode_(lin_log_mode)
        , edge_weight_influence_(edge_weight_influence)
        , barnes_hut_optimize_(barnes_hut_optimize)
        , barnes_hut_theta_(barnes_hut_theta)
        , thread_count_(thread_count) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override {
        CoordinateMap coordinates;
        const auto& nodes = input_graph.getNodes();
        if (nodes.empty()) {
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(nodes.size());
        for (const auto& [node_id, node] : nodes) {
            (void)node;
            node_ids.push_back(node_id);
        }
        std::sort(node_ids.begin(), node_ids.end());

        if (progress_callback) {
            progress_callback(0.0);
        }

        if (node_ids.size() == 1) {
            coordinates.emplace(node_ids.front(), Coordinate{width_ * 0.5, height_ * 0.5});
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::unordered_map<graph::NodeId, std::size_t> node_index;
        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            node_index.emplace(node_ids[i], i);
        }

        std::vector<Coordinate> positions(node_ids.size());
        initialize_positions(positions, width_, height_);

        std::vector<LayoutData> layout_data(node_ids.size());
        const std::size_t effective_iterations = std::max<std::size_t>(iterations_, 1);
        const auto& edges = input_graph.getEdges();
        double speed = 1.0;

        for (std::size_t iteration = 0; iteration < effective_iterations; ++iteration) {
            std::vector<Coordinate> displacements(node_ids.size(), Coordinate{});
            std::unique_ptr<QuadTree> quadtree;
            if (barnes_hut_optimize_ && node_ids.size() >= 32) {
                quadtree = build_quadtree(positions);
            }

            parallel_for(node_ids.size(), [&](std::size_t begin, std::size_t end, std::vector<Coordinate>& local) {
                for (std::size_t i = begin; i < end; ++i) {
                    if (quadtree) {
                        quadtree->apply_repulsion(i, positions, barnes_hut_theta_, scaling_ratio_, local);
                    } else {
                        for (std::size_t j = 0; j < positions.size(); ++j) {
                            if (i == j) {
                                continue;
                            }
                            apply_pairwise_repulsion(i, j, positions, scaling_ratio_, local);
                        }
                    }

                    const double dx = positions[i].x - width_ * 0.5;
                    const double dy = positions[i].y - height_ * 0.5;
                    local[i].x -= dx * gravity_ * 0.01;
                    local[i].y -= dy * gravity_ * 0.01;
                }
            }, displacements);

            parallel_for_edges(edges, node_index, [&](const std::vector<const graph::Edge*>& chunk,
                                                      std::vector<Coordinate>& local) {
                for (const auto* edge : chunk) {
                    const auto src_it = node_index.find(edge->source);
                    const auto tgt_it = node_index.find(edge->target);
                    if (src_it == node_index.end() || tgt_it == node_index.end()) {
                        continue;
                    }
                    apply_attraction(src_it->second, tgt_it->second, *edge, positions, local);
                }
            }, displacements);

            double global_swinging = 0.0;
            double global_traction = 0.0;
            for (std::size_t i = 0; i < positions.size(); ++i) {
                const double swing_x = displacements[i].x - layout_data[i].old_dx;
                const double swing_y = displacements[i].y - layout_data[i].old_dy;
                const double tract_x = displacements[i].x + layout_data[i].old_dx;
                const double tract_y = displacements[i].y + layout_data[i].old_dy;
                const double swinging = std::sqrt(swing_x * swing_x + swing_y * swing_y);
                const double traction = std::sqrt(tract_x * tract_x + tract_y * tract_y) * 0.5;
                layout_data[i].swinging = swinging;
                layout_data[i].traction = traction;
                global_swinging += swinging;
                global_traction += traction;
            }

            const double target_speed = global_swinging <= min_distance()
                ? speed
                : std::max((global_traction + min_distance()) / (global_swinging + min_distance()), 0.01);
            if (target_speed > speed) {
                speed += std::min(target_speed - speed, speed * 0.5);
            } else {
                speed = target_speed;
            }
            speed = std::max(speed, 0.01);

            for (std::size_t i = 0; i < positions.size(); ++i) {
                const double factor = speed / (1.0 + speed * std::sqrt(layout_data[i].swinging + min_distance()));
                positions[i].x += displacements[i].x * factor;
                positions[i].y += displacements[i].y * factor;
                layout_data[i].old_dx = displacements[i].x;
                layout_data[i].old_dy = displacements[i].y;
            }

            const double progress = static_cast<double>(iteration + 1) /
                                    static_cast<double>(effective_iterations);
            if (progress_callback) {
                progress_callback(progress);
            }
        }

        rescale_positions(positions, margin_, std::max(width_ - margin_, margin_),
                          margin_, std::max(height_ - margin_, margin_));

        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            coordinates.emplace(node_ids[i], positions[i]);
        }

        if (progress_callback) {
            progress_callback(1.0);
        }

        return coordinates;
    }

private:
    struct LayoutData {
        double old_dx{0.0};
        double old_dy{0.0};
        double swinging{0.0};
        double traction{0.0};
    };

    struct QuadTree {
        double min_x;
        double min_y;
        double max_x;
        double max_y;
        double mass_x{0.0};
        double mass_y{0.0};
        double mass{0.0};
        std::vector<std::size_t> indices;
        std::array<std::unique_ptr<QuadTree>, 4> children;

        QuadTree(double x0, double y0, double x1, double y1)
            : min_x(x0), min_y(y0), max_x(x1), max_y(y1) {}

        bool is_leaf() const {
            return std::all_of(children.begin(), children.end(), [](const auto& child) {
                return child == nullptr;
            });
        }

        double size() const { return std::max(max_x - min_x, max_y - min_y); }

        void insert(std::size_t index, const std::vector<Coordinate>& positions) {
            const auto& position = positions[index];
            const double new_mass = mass + 1.0;
            mass_x = ((mass_x * mass) + position.x) / new_mass;
            mass_y = ((mass_y * mass) + position.y) / new_mass;
            mass = new_mass;

            if (is_leaf() && indices.size() < 1) {
                indices.push_back(index);
                return;
            }

            if (is_leaf()) {
                subdivide();
                const auto existing = indices.front();
                indices.clear();
                child_for(positions[existing])->insert(existing, positions);
            }

            child_for(position)->insert(index, positions);
        }

        void apply_repulsion(std::size_t index,
                             const std::vector<Coordinate>& positions,
                             double theta,
                             double scaling_ratio,
                             std::vector<Coordinate>& displacements) const {
            if (mass <= 0.0) {
                return;
            }

            const auto& position = positions[index];
            if (is_leaf() && indices.size() == 1 && indices.front() == index) {
                return;
            }

            const double dx = position.x - mass_x;
            const double dy = position.y - mass_y;
            const double distance = std::max(std::sqrt(dx * dx + dy * dy), min_distance());

            if (is_leaf() || (size() / distance) < theta) {
                const double force = scaling_ratio * mass / (distance * distance);
                displacements[index].x += (dx / distance) * force;
                displacements[index].y += (dy / distance) * force;
                return;
            }

            for (const auto& child : children) {
                if (child) {
                    child->apply_repulsion(index, positions, theta, scaling_ratio, displacements);
                }
            }
        }

    private:
        void subdivide() {
            const double mid_x = (min_x + max_x) * 0.5;
            const double mid_y = (min_y + max_y) * 0.5;
            children[0] = std::make_unique<QuadTree>(min_x, min_y, mid_x, mid_y);
            children[1] = std::make_unique<QuadTree>(mid_x, min_y, max_x, mid_y);
            children[2] = std::make_unique<QuadTree>(min_x, mid_y, mid_x, max_y);
            children[3] = std::make_unique<QuadTree>(mid_x, mid_y, max_x, max_y);
        }

        QuadTree* child_for(const Coordinate& position) {
            const double mid_x = (min_x + max_x) * 0.5;
            const double mid_y = (min_y + max_y) * 0.5;
            const int quadrant = (position.x >= mid_x ? 1 : 0) + (position.y >= mid_y ? 2 : 0);
            return children[quadrant].get();
        }
    };

    static double min_distance() noexcept { return 1e-9; }

    static std::size_t default_thread_count() {
        const auto hc = std::thread::hardware_concurrency();
        return std::max<std::size_t>(1, hc == 0 ? 2 : hc);
    }

    static void initialize_positions(std::vector<Coordinate>& positions, double width, double height) {
        const double center_x = width * 0.5;
        const double center_y = height * 0.5;
        const double radius = std::max(std::min(width, height) * 0.35, 1.0);
        const double tau = 6.28318530717958647692;
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const double angle = tau * static_cast<double>(i) / static_cast<double>(positions.size());
            positions[i] = Coordinate{center_x + radius * std::cos(angle),
                                      center_y + radius * std::sin(angle)};
        }
    }

    static std::unique_ptr<QuadTree> build_quadtree(const std::vector<Coordinate>& positions) {
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }
        if (max_x - min_x <= min_distance()) {
            max_x = min_x + 1.0;
        }
        if (max_y - min_y <= min_distance()) {
            max_y = min_y + 1.0;
        }
        auto root = std::make_unique<QuadTree>(min_x, min_y, max_x, max_y);
        for (std::size_t i = 0; i < positions.size(); ++i) {
            root->insert(i, positions);
        }
        return root;
    }

    static void apply_pairwise_repulsion(std::size_t i,
                                         std::size_t j,
                                         const std::vector<Coordinate>& positions,
                                         double scaling_ratio,
                                         std::vector<Coordinate>& local) {
        const double dx = positions[i].x - positions[j].x;
        const double dy = positions[i].y - positions[j].y;
        const double distance = std::max(std::sqrt(dx * dx + dy * dy), min_distance());
        const double force = scaling_ratio / (distance * distance);
        local[i].x += (dx / distance) * force;
        local[i].y += (dy / distance) * force;
    }

    void apply_attraction(std::size_t source,
                          std::size_t target,
                          const graph::Edge& edge,
                          const std::vector<Coordinate>& positions,
                          std::vector<Coordinate>& local) const {
        const double dx = positions[source].x - positions[target].x;
        const double dy = positions[source].y - positions[target].y;
        const double distance = std::max(std::sqrt(dx * dx + dy * dy), min_distance());
        const double edge_weight = std::max(edge.weight, 0.0);
        const double influenced_weight = edge_weight_influence_ == 0.0
            ? 1.0
            : (edge_weight_influence_ == 1.0 ? edge_weight : std::pow(edge_weight, edge_weight_influence_));
        const double attraction = lin_log_mode_
            ? influenced_weight * std::log1p(distance)
            : influenced_weight * distance;
        const double scaled = attraction / std::max(scaling_ratio_, 1.0);
        const double fx = (dx / distance) * scaled;
        const double fy = (dy / distance) * scaled;
        local[source].x -= fx;
        local[source].y -= fy;
        local[target].x += fx;
        local[target].y += fy;
    }

    template<typename Worker>
    void parallel_for(std::size_t count,
                      Worker worker,
                      std::vector<Coordinate>& aggregate) const {
        const std::size_t workers = std::min<std::size_t>(std::max<std::size_t>(1, thread_count_), count);
        std::vector<std::vector<Coordinate>> locals(workers, std::vector<Coordinate>(count, Coordinate{}));
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for (std::size_t worker_index = 0; worker_index < workers; ++worker_index) {
            const std::size_t begin = worker_index * count / workers;
            const std::size_t end = (worker_index + 1) * count / workers;
            threads.emplace_back([&, worker_index, begin, end] {
                worker(begin, end, locals[worker_index]);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        for (const auto& local : locals) {
            for (std::size_t i = 0; i < aggregate.size(); ++i) {
                aggregate[i].x += local[i].x;
                aggregate[i].y += local[i].y;
            }
        }
    }

    template<typename Worker>
    void parallel_for_edges(const graph::Graph<graph::Node, graph::Edge>::EdgeMap& edges,
                            const std::unordered_map<graph::NodeId, std::size_t>&,
                            Worker worker,
                            std::vector<Coordinate>& aggregate) const {
        std::vector<const graph::Edge*> edge_list;
        edge_list.reserve(edges.size());
        for (const auto& [edge_id, edge] : edges) {
            (void)edge_id;
            edge_list.push_back(&edge);
        }
        const std::size_t workers = std::min<std::size_t>(std::max<std::size_t>(1, thread_count_), std::max<std::size_t>(edge_list.size(), 1));
        std::vector<std::vector<Coordinate>> locals(workers, std::vector<Coordinate>(aggregate.size(), Coordinate{}));
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for (std::size_t worker_index = 0; worker_index < workers; ++worker_index) {
            const std::size_t begin = worker_index * edge_list.size() / workers;
            const std::size_t end = (worker_index + 1) * edge_list.size() / workers;
            threads.emplace_back([&, worker_index, begin, end] {
                std::vector<const graph::Edge*> chunk;
                chunk.reserve(end - begin);
                for (std::size_t i = begin; i < end; ++i) {
                    chunk.push_back(edge_list[i]);
                }
                worker(chunk, locals[worker_index]);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        for (const auto& local : locals) {
            for (std::size_t i = 0; i < aggregate.size(); ++i) {
                aggregate[i].x += local[i].x;
                aggregate[i].y += local[i].y;
            }
        }
    }

    static void rescale_positions(std::vector<Coordinate>& positions,
                                  double target_min_x,
                                  double target_max_x,
                                  double target_min_y,
                                  double target_max_y) {
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }
        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double target_span_x = target_max_x - target_min_x;
        const double target_span_y = target_max_y - target_min_y;
        for (auto& position : positions) {
            position.x = (span_x <= min_distance())
                ? (target_min_x + target_max_x) * 0.5
                : target_min_x + ((position.x - min_x) / span_x) * target_span_x;
            position.y = (span_y <= min_distance())
                ? (target_min_y + target_max_y) * 0.5
                : target_min_y + ((position.y - min_y) / span_y) * target_span_y;
        }
    }

    std::size_t iterations_;
    double width_;
    double height_;
    double margin_;
    double scaling_ratio_;
    double gravity_;
    bool lin_log_mode_;
    double edge_weight_influence_;
    bool barnes_hut_optimize_;
    double barnes_hut_theta_;
    std::size_t thread_count_;
};

class KamadaKawaiLayout : public Layout {
public:
    explicit KamadaKawaiLayout(std::size_t iterations = 200,
                               double width = 1024.0,
                               double height = 1024.0,
                               double margin = 0.0,
                               double spring_constant = 1.0)
        : iterations_(iterations)
        , width_(width)
        , height_(height)
        , margin_(margin)
        , spring_constant_(spring_constant) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override {
        CoordinateMap coordinates;
        const auto& nodes = input_graph.getNodes();
        if (nodes.empty()) {
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(nodes.size());
        for (const auto& [node_id, node] : nodes) {
            (void)node;
            node_ids.push_back(node_id);
        }
        std::sort(node_ids.begin(), node_ids.end());

        if (progress_callback) {
            progress_callback(0.0);
        }

        if (node_ids.size() == 1) {
            coordinates.emplace(node_ids.front(), Coordinate{width_ * 0.5, height_ * 0.5});
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        const auto components = connected_components(input_graph, node_ids);
        const double component_width = width_ / static_cast<double>(std::max<std::size_t>(components.size(), 1));
        const double component_gap = std::max(component_width * 0.1, 20.0);

        std::vector<Coordinate> global_positions(node_ids.size(), Coordinate{});
        for (std::size_t component_index = 0; component_index < components.size(); ++component_index) {
            const auto& component = components[component_index];
            std::vector<Coordinate> local_positions(component.size(), Coordinate{});
            initialize_component_positions(local_positions,
                                           component_width - component_gap,
                                           height_ - 2.0 * margin_);

            const auto distances = all_pairs_shortest_paths(input_graph, node_ids, component);
            const double diameter = graph_diameter(distances);
            const double optimal_distance = std::max(std::min(component_width - component_gap,
                                                              height_ - 2.0 * margin_) /
                                                         std::max(diameter, 1.0),
                                                     1.0);

            const std::size_t effective_iterations = std::max<std::size_t>(iterations_, 1);
            for (std::size_t iteration = 0; iteration < effective_iterations; ++iteration) {
                const double learning_rate = 0.1 *
                    (1.0 - static_cast<double>(iteration) / static_cast<double>(effective_iterations));

                for (std::size_t i = 0; i < component.size(); ++i) {
                    double gradient_x = 0.0;
                    double gradient_y = 0.0;

                    for (std::size_t j = 0; j < component.size(); ++j) {
                        if (i == j || !std::isfinite(distances[i][j])) {
                            continue;
                        }

                        const double dx = local_positions[i].x - local_positions[j].x;
                        const double dy = local_positions[i].y - local_positions[j].y;
                        const double distance = std::max(euclidean_distance(dx, dy), min_distance());
                        const double dij = std::max(distances[i][j], 1.0);
                        const double ideal_length = optimal_distance * dij;
                        const double spring = spring_constant_ / (dij * dij);
                        const double factor = spring * (1.0 - ideal_length / distance);

                        gradient_x += factor * dx;
                        gradient_y += factor * dy;
                    }

                    local_positions[i].x -= gradient_x * learning_rate;
                    local_positions[i].y -= gradient_y * learning_rate;
                }

                const double base_progress = static_cast<double>(component_index);
                const double component_progress = static_cast<double>(iteration + 1) /
                    static_cast<double>(effective_iterations);
                const double progress = (base_progress + component_progress) /
                    static_cast<double>(std::max<std::size_t>(components.size(), 1));

                if (progress_callback) {
                    progress_callback(progress);
                }
            }

            rescale_positions(local_positions,
                              0.0,
                              std::max(component_width - component_gap, 1.0),
                              margin_,
                              std::max(height_ - margin_, margin_));

            const double x_offset = component_index * component_width + component_gap * 0.5;
            for (std::size_t i = 0; i < component.size(); ++i) {
                const auto global_index = component[i];
                global_positions[global_index] = Coordinate{local_positions[i].x + x_offset,
                                                            local_positions[i].y};
            }
        }

        rescale_positions(global_positions,
                          margin_,
                          std::max(width_ - margin_, margin_),
                          margin_,
                          std::max(height_ - margin_, margin_));

        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            coordinates.emplace(node_ids[i], global_positions[i]);
        }

        if (progress_callback) {
            progress_callback(1.0);
        }

        return coordinates;
    }

private:
    static double min_distance() noexcept { return 1e-9; }

    static double euclidean_distance(double dx, double dy) noexcept {
        return std::sqrt(dx * dx + dy * dy);
    }

    static std::vector<std::vector<std::size_t>> connected_components(
        const graph::Graph<graph::Node, graph::Edge>& input_graph,
        const std::vector<graph::NodeId>& node_ids)
    {
        std::unordered_map<graph::NodeId, std::size_t> index_by_id;
        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            index_by_id.emplace(node_ids[i], i);
        }

        std::vector<bool> visited(node_ids.size(), false);
        std::vector<std::vector<std::size_t>> components;

        for (std::size_t start = 0; start < node_ids.size(); ++start) {
            if (visited[start]) {
                continue;
            }

            std::vector<std::size_t> component;
            std::vector<std::size_t> stack{start};
            visited[start] = true;

            while (!stack.empty()) {
                const std::size_t current_index = stack.back();
                stack.pop_back();
                component.push_back(current_index);

                const auto node_it = input_graph.getNodes().find(node_ids[current_index]);
                if (node_it == input_graph.getNodes().end()) {
                    continue;
                }

                for (graph::EdgeId edge_id : node_it->second.edges) {
                    const auto edge_it = input_graph.getEdges().find(edge_id);
                    if (edge_it == input_graph.getEdges().end()) {
                        continue;
                    }

                    const auto neighbor_id = edge_it->second.source == node_ids[current_index]
                        ? edge_it->second.target
                        : edge_it->second.source;

                    const auto neighbor_it = index_by_id.find(neighbor_id);
                    if (neighbor_it == index_by_id.end() || visited[neighbor_it->second]) {
                        continue;
                    }

                    visited[neighbor_it->second] = true;
                    stack.push_back(neighbor_it->second);
                }
            }

            std::sort(component.begin(), component.end());
            components.push_back(std::move(component));
        }

        return components;
    }

    std::vector<std::vector<double>> all_pairs_shortest_paths(
        const graph::Graph<graph::Node, graph::Edge>& input_graph,
        const std::vector<graph::NodeId>& node_ids,
        const std::vector<std::size_t>& component) const
    {
        const double inf = std::numeric_limits<double>::infinity();
        std::vector<std::vector<double>> distances(
            component.size(), std::vector<double>(component.size(), inf));

        std::unordered_map<graph::NodeId, std::size_t> local_index;
        for (std::size_t i = 0; i < component.size(); ++i) {
            const auto node_id = node_ids[component[i]];
            local_index.emplace(node_id, i);
            distances[i][i] = 0.0;
        }

        for (const auto& [edge_id, edge] : input_graph.getEdges()) {
            (void)edge_id;
            const auto src_it = local_index.find(edge.source);
            const auto tgt_it = local_index.find(edge.target);
            if (src_it == local_index.end() || tgt_it == local_index.end()) {
                continue;
            }

            const double weight = edge.weight > 0.0 ? edge.weight : 1.0;
            const double distance = 1.0 / weight;
            distances[src_it->second][tgt_it->second] =
                std::min(distances[src_it->second][tgt_it->second], distance);
            distances[tgt_it->second][src_it->second] =
                std::min(distances[tgt_it->second][src_it->second], distance);
        }

        for (std::size_t k = 0; k < component.size(); ++k) {
            for (std::size_t i = 0; i < component.size(); ++i) {
                for (std::size_t j = 0; j < component.size(); ++j) {
                    if (!std::isfinite(distances[i][k]) || !std::isfinite(distances[k][j])) {
                        continue;
                    }
                    distances[i][j] = std::min(distances[i][j],
                                               distances[i][k] + distances[k][j]);
                }
            }
        }

        return distances;
    }

    static double graph_diameter(const std::vector<std::vector<double>>& distances) {
        double diameter = 1.0;
        for (const auto& row : distances) {
            for (double value : row) {
                if (std::isfinite(value)) {
                    diameter = std::max(diameter, value);
                }
            }
        }
        return diameter;
    }

    static void initialize_component_positions(std::vector<Coordinate>& positions,
                                               double width,
                                               double height) {
        const double center_x = width * 0.5;
        const double center_y = height * 0.5;
        const double radius = std::max(std::min(width, height) * 0.35, 1.0);
        const double tau = 6.28318530717958647692;

        for (std::size_t i = 0; i < positions.size(); ++i) {
            const double angle = tau * static_cast<double>(i) /
                                 static_cast<double>(positions.size());
            positions[i] = Coordinate{center_x + radius * std::cos(angle),
                                      center_y + radius * std::sin(angle)};
        }
    }

    static void rescale_positions(std::vector<Coordinate>& positions,
                                  double target_min_x,
                                  double target_max_x,
                                  double target_min_y,
                                  double target_max_y) {
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();

        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }

        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double target_span_x = target_max_x - target_min_x;
        const double target_span_y = target_max_y - target_min_y;

        for (auto& position : positions) {
            position.x = (span_x <= min_distance())
                ? (target_min_x + target_max_x) * 0.5
                : target_min_x + ((position.x - min_x) / span_x) * target_span_x;
            position.y = (span_y <= min_distance())
                ? (target_min_y + target_max_y) * 0.5
                : target_min_y + ((position.y - min_y) / span_y) * target_span_y;
        }
    }

    std::size_t iterations_;
    double width_;
    double height_;
    double margin_;
    double spring_constant_;
};

class CommunityLayoutSupport;

class CircularLayout : public Layout {
public:
    explicit CircularLayout(double width = 1024.0,
                            double height = 1024.0,
                            double margin = 0.0);

    void setCommunityAssignment(std::unordered_map<graph::NodeId, graph::CommunityId> assignment);

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override;

private:
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment_;
    double width_;
    double height_;
    double margin_;
};

class GridLayout : public Layout {
public:
    explicit GridLayout(double width = 1024.0,
                        double height = 1024.0,
                        double margin = 0.0);

    void setCommunityAssignment(std::unordered_map<graph::NodeId, graph::CommunityId> assignment);

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override;

private:
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment_;
    double width_;
    double height_;
    double margin_;
};

class GridKamadaKawaiLayout : public Layout {
public:
    explicit GridKamadaKawaiLayout(double width = 1024.0,
                                   double height = 1024.0,
                                   double margin = 0.0);

    void setCommunityAssignment(std::unordered_map<graph::NodeId, graph::CommunityId> assignment);

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override;

private:
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment_;
    double width_;
    double height_;
    double margin_;
};

class ForceAtlas3DLayout : public Layout {
public:
    explicit ForceAtlas3DLayout(std::size_t iterations = 200,
                                double width = 1024.0,
                                double height = 1024.0,
                                double depth = 1024.0,
                                double margin = 0.0,
                                double scaling_ratio = 10.0,
                                double gravity = 1.0,
                                bool lin_log_mode = false,
                                double edge_weight_influence = 1.0,
                                std::size_t thread_count = default_thread_count())
        : iterations_(iterations)
        , width_(width)
        , height_(height)
        , depth_(depth)
        , margin_(margin)
        , scaling_ratio_(scaling_ratio)
        , gravity_(gravity)
        , lin_log_mode_(lin_log_mode)
        , edge_weight_influence_(edge_weight_influence)
        , thread_count_(thread_count) {}

    Coordinate3DMap compute3D(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                               ProgressCallback progress_callback = nullptr) {
        Coordinate3DMap coordinates;
        const auto& nodes = input_graph.getNodes();
        if (nodes.empty()) {
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(nodes.size());
        for (const auto& [node_id, node] : nodes) {
            (void)node;
            node_ids.push_back(node_id);
        }
        std::sort(node_ids.begin(), node_ids.end());

        if (progress_callback) {
            progress_callback(0.0);
        }

        if (node_ids.size() == 1) {
            coordinates.emplace(node_ids.front(),
                                Coordinate3D{width_ * 0.5, height_ * 0.5, depth_ * 0.5});
            if (progress_callback) {
                progress_callback(1.0);
            }
            return coordinates;
        }

        std::unordered_map<graph::NodeId, std::size_t> node_index;
        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            node_index.emplace(node_ids[i], i);
        }

        std::vector<Coordinate3D> positions(node_ids.size());
        initialize_positions_3d(positions, width_, height_, depth_);

        std::vector<LayoutData3D> layout_data(node_ids.size());
        const std::size_t effective_iterations = std::max<std::size_t>(iterations_, 1);
        const auto& edges = input_graph.getEdges();
        double speed = 1.0;

        for (std::size_t iteration = 0; iteration < effective_iterations; ++iteration) {
            std::vector<Coordinate3D> displacements(node_ids.size(), Coordinate3D{});

            parallel_for_3d(node_ids.size(),
                [&](std::size_t begin, std::size_t end, std::vector<Coordinate3D>& local) {
                    for (std::size_t i = begin; i < end; ++i) {
                        for (std::size_t j = 0; j < positions.size(); ++j) {
                            if (i == j) {
                                continue;
                            }
                            apply_pairwise_repulsion_3d(i, j, positions, scaling_ratio_, local);
                        }

                        const double dx = positions[i].x - width_ * 0.5;
                        const double dy = positions[i].y - height_ * 0.5;
                        const double dz = positions[i].z - depth_ * 0.5;
                        local[i].x -= dx * gravity_ * 0.01;
                        local[i].y -= dy * gravity_ * 0.01;
                        local[i].z -= dz * gravity_ * 0.01;
                    }
                }, displacements);

            parallel_for_edges_3d(edges, node_index,
                [&](const std::vector<const graph::Edge*>& chunk,
                    std::vector<Coordinate3D>& local) {
                    for (const auto* edge : chunk) {
                        const auto src_it = node_index.find(edge->source);
                        const auto tgt_it = node_index.find(edge->target);
                        if (src_it == node_index.end() || tgt_it == node_index.end()) {
                            continue;
                        }
                        apply_attraction_3d(src_it->second, tgt_it->second,
                                            *edge, positions, local);
                    }
                }, displacements);

            double global_swinging = 0.0;
            double global_traction = 0.0;
            for (std::size_t i = 0; i < positions.size(); ++i) {
                const double swing_x = displacements[i].x - layout_data[i].old_dx;
                const double swing_y = displacements[i].y - layout_data[i].old_dy;
                const double swing_z = displacements[i].z - layout_data[i].old_dz;
                const double tract_x = displacements[i].x + layout_data[i].old_dx;
                const double tract_y = displacements[i].y + layout_data[i].old_dy;
                const double tract_z = displacements[i].z + layout_data[i].old_dz;
                const double swinging = std::sqrt(swing_x * swing_x +
                                                  swing_y * swing_y +
                                                  swing_z * swing_z);
                const double traction = std::sqrt(tract_x * tract_x +
                                                  tract_y * tract_y +
                                                  tract_z * tract_z) * 0.5;
                layout_data[i].swinging = swinging;
                layout_data[i].traction = traction;
                global_swinging += swinging;
                global_traction += traction;
            }

            const double target_speed = global_swinging <= min_distance()
                ? speed
                : std::max((global_traction + min_distance()) /
                           (global_swinging + min_distance()), 0.01);
            if (target_speed > speed) {
                speed += std::min(target_speed - speed, speed * 0.5);
            } else {
                speed = target_speed;
            }
            speed = std::max(speed, 0.01);

            for (std::size_t i = 0; i < positions.size(); ++i) {
                const double factor = speed / (1.0 + speed *
                    std::sqrt(layout_data[i].swinging + min_distance()));
                positions[i].x += displacements[i].x * factor;
                positions[i].y += displacements[i].y * factor;
                positions[i].z += displacements[i].z * factor;
                layout_data[i].old_dx = displacements[i].x;
                layout_data[i].old_dy = displacements[i].y;
                layout_data[i].old_dz = displacements[i].z;
            }

            const double progress = static_cast<double>(iteration + 1) /
                                    static_cast<double>(effective_iterations);
            if (progress_callback) {
                progress_callback(progress);
            }
        }

        rescale_positions_3d(positions,
                             margin_, std::max(width_ - margin_, margin_),
                             margin_, std::max(height_ - margin_, margin_),
                             margin_, std::max(depth_ - margin_, margin_));

        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            coordinates.emplace(node_ids[i], positions[i]);
        }

        if (progress_callback) {
            progress_callback(1.0);
        }

        return coordinates;
    }

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override {
        auto coords3d = compute3D(input_graph, progress_callback);
        CoordinateMap coords2d;
        for (const auto& [id, c3d] : coords3d) {
            coords2d.emplace(id, Coordinate{c3d.x, c3d.y});
        }
        return coords2d;
    }

private:
    struct LayoutData3D {
        double old_dx{0.0};
        double old_dy{0.0};
        double old_dz{0.0};
        double swinging{0.0};
        double traction{0.0};
    };

    static double min_distance() noexcept { return 1e-9; }

    static std::size_t default_thread_count() {
        const auto hc = std::thread::hardware_concurrency();
        return std::max<std::size_t>(1, hc == 0 ? 2 : hc);
    }

    static void initialize_positions_3d(std::vector<Coordinate3D>& positions,
                                         double width, double height, double depth) {
        const double center_x = width * 0.5;
        const double center_y = height * 0.5;
        const double center_z = depth * 0.5;
        const double radius = std::max({width, height, depth}) * 0.35;
        const double golden_angle = 2.39996322972865332;
        const auto n = static_cast<double>(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const double y = 1.0 - 2.0 * static_cast<double>(i) / n;
            const double r = std::sqrt(std::max(1.0 - y * y, 0.0));
            const double theta = golden_angle * static_cast<double>(i);
            positions[i] = Coordinate3D{
                center_x + radius * r * std::cos(theta),
                center_y + radius * r * std::sin(theta),
                center_z + radius * y
            };
        }
    }

    static void apply_pairwise_repulsion_3d(std::size_t i,
                                             std::size_t j,
                                             const std::vector<Coordinate3D>& positions,
                                             double scaling_ratio,
                                             std::vector<Coordinate3D>& local) {
        const double dx = positions[i].x - positions[j].x;
        const double dy = positions[i].y - positions[j].y;
        const double dz = positions[i].z - positions[j].z;
        const double distance = std::max(
            std::sqrt(dx * dx + dy * dy + dz * dz), min_distance());
        const double force = scaling_ratio / (distance * distance);
        local[i].x += (dx / distance) * force;
        local[i].y += (dy / distance) * force;
        local[i].z += (dz / distance) * force;
    }

    void apply_attraction_3d(std::size_t source,
                              std::size_t target,
                              const graph::Edge& edge,
                              const std::vector<Coordinate3D>& positions,
                              std::vector<Coordinate3D>& local) const {
        const double dx = positions[source].x - positions[target].x;
        const double dy = positions[source].y - positions[target].y;
        const double dz = positions[source].z - positions[target].z;
        const double distance = std::max(
            std::sqrt(dx * dx + dy * dy + dz * dz), min_distance());
        const double edge_weight = std::max(edge.weight, 0.0);
        const double influenced_weight = edge_weight_influence_ == 0.0
            ? 1.0
            : (edge_weight_influence_ == 1.0
               ? edge_weight
               : std::pow(edge_weight, edge_weight_influence_));
        const double attraction = lin_log_mode_
            ? influenced_weight * std::log1p(distance)
            : influenced_weight * distance;
        const double scaled = attraction / std::max(scaling_ratio_, 1.0);
        const double fx = (dx / distance) * scaled;
        const double fy = (dy / distance) * scaled;
        const double fz = (dz / distance) * scaled;
        local[source].x -= fx;
        local[source].y -= fy;
        local[source].z -= fz;
        local[target].x += fx;
        local[target].y += fy;
        local[target].z += fz;
    }

    template<typename Worker>
    void parallel_for_3d(std::size_t count,
                          Worker worker,
                          std::vector<Coordinate3D>& aggregate) const {
        const std::size_t workers =
            std::min<std::size_t>(std::max<std::size_t>(1, thread_count_), count);
        std::vector<std::vector<Coordinate3D>> locals(
            workers, std::vector<Coordinate3D>(count, Coordinate3D{}));
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for (std::size_t worker_index = 0; worker_index < workers; ++worker_index) {
            const std::size_t begin = worker_index * count / workers;
            const std::size_t end = (worker_index + 1) * count / workers;
            threads.emplace_back([&, worker_index, begin, end] {
                worker(begin, end, locals[worker_index]);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        for (const auto& local : locals) {
            for (std::size_t i = 0; i < aggregate.size(); ++i) {
                aggregate[i].x += local[i].x;
                aggregate[i].y += local[i].y;
                aggregate[i].z += local[i].z;
            }
        }
    }

    template<typename Worker>
    void parallel_for_edges_3d(
            const graph::Graph<graph::Node, graph::Edge>::EdgeMap& edges,
            const std::unordered_map<graph::NodeId, std::size_t>&,
            Worker worker,
            std::vector<Coordinate3D>& aggregate) const {
        std::vector<const graph::Edge*> edge_list;
        edge_list.reserve(edges.size());
        for (const auto& [edge_id, edge] : edges) {
            (void)edge_id;
            edge_list.push_back(&edge);
        }
        const std::size_t workers =
            std::min<std::size_t>(std::max<std::size_t>(1, thread_count_),
                                  std::max<std::size_t>(edge_list.size(), 1));
        std::vector<std::vector<Coordinate3D>> locals(
            workers, std::vector<Coordinate3D>(aggregate.size(), Coordinate3D{}));
        std::vector<std::thread> threads;
        threads.reserve(workers);
        for (std::size_t worker_index = 0; worker_index < workers; ++worker_index) {
            const std::size_t begin = worker_index * edge_list.size() / workers;
            const std::size_t end = (worker_index + 1) * edge_list.size() / workers;
            threads.emplace_back([&, worker_index, begin, end] {
                std::vector<const graph::Edge*> chunk;
                chunk.reserve(end - begin);
                for (std::size_t i = begin; i < end; ++i) {
                    chunk.push_back(edge_list[i]);
                }
                worker(chunk, locals[worker_index]);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        for (const auto& local : locals) {
            for (std::size_t i = 0; i < aggregate.size(); ++i) {
                aggregate[i].x += local[i].x;
                aggregate[i].y += local[i].y;
                aggregate[i].z += local[i].z;
            }
        }
    }

    static void rescale_positions_3d(std::vector<Coordinate3D>& positions,
                                      double target_min_x, double target_max_x,
                                      double target_min_y, double target_max_y,
                                      double target_min_z, double target_max_z) {
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();
        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
            min_z = std::min(min_z, position.z);
            max_z = std::max(max_z, position.z);
        }
        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double span_z = max_z - min_z;
        const double target_span_x = target_max_x - target_min_x;
        const double target_span_y = target_max_y - target_min_y;
        const double target_span_z = target_max_z - target_min_z;
        for (auto& position : positions) {
            position.x = (span_x <= min_distance())
                ? (target_min_x + target_max_x) * 0.5
                : target_min_x + ((position.x - min_x) / span_x) * target_span_x;
            position.y = (span_y <= min_distance())
                ? (target_min_y + target_max_y) * 0.5
                : target_min_y + ((position.y - min_y) / span_y) * target_span_y;
            position.z = (span_z <= min_distance())
                ? (target_min_z + target_max_z) * 0.5
                : target_min_z + ((position.z - min_z) / span_z) * target_span_z;
        }
    }

    std::size_t iterations_;
    double width_;
    double height_;
    double depth_;
    double margin_;
    double scaling_ratio_;
    double gravity_;
    bool lin_log_mode_;
    double edge_weight_influence_;
    std::size_t thread_count_;
};

class CommunityForceAtlas2Layout : public Layout {
public:
    explicit CommunityForceAtlas2Layout(double width = 1024.0,
                                        double height = 1024.0,
                                        double margin = 0.0);

    void setCommunityAssignment(std::unordered_map<graph::NodeId, graph::CommunityId> assignment);

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override;

private:
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment_;
    double width_;
    double height_;
    double margin_;
};

struct QuotientGraph {
    graph::Graph<graph::Node, graph::Edge> graph;
    std::map<graph::CommunityId, graph::NodeId> community_to_node;
    std::vector<std::vector<graph::NodeId>> communities;
};

class CommunityLayoutSupport {
public:
    using AssignmentMap = std::unordered_map<graph::NodeId, graph::CommunityId>;

    template<typename LocalLayoutFactory, typename CenterFactory>
    static CoordinateMap compose(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                                 const AssignmentMap& explicit_assignment,
                                 LocalLayoutFactory local_layout_factory,
                                 CenterFactory center_factory,
                                 double width,
                                 double height,
                                 double margin,
                                 ProgressCallback progress_callback) {
        CoordinateMap coordinates;
        const auto assignment = normalize_assignment(input_graph, explicit_assignment);
        const auto communities = bucket_nodes(assignment);
        const auto centers = center_factory(communities.size());
        if (progress_callback) {
            progress_callback(0.0);
        }

        const double local_width = std::max(width / std::max<std::size_t>(communities.size(), 1), 120.0);
        const double local_height = std::max(height / std::max<std::size_t>(communities.size(), 1), 120.0);

        for (std::size_t community_index = 0; community_index < communities.size(); ++community_index) {
            const auto subgraph = induced_subgraph(input_graph, communities[community_index]);
            auto local_layout = local_layout_factory(local_width, local_height);
            const auto local_coordinates = local_layout.compute(subgraph);

            const auto& center = centers[community_index];
            for (const auto& node_id : communities[community_index]) {
                const auto it = local_coordinates.find(node_id);
                if (it == local_coordinates.end()) {
                    continue;
                }
                coordinates[node_id] = Coordinate{
                    it->second.x + center.x - (local_width * 0.5),
                    it->second.y + center.y - (local_height * 0.5)
                };
            }

            if (progress_callback) {
                progress_callback(static_cast<double>(community_index + 1) /
                                  static_cast<double>(std::max<std::size_t>(communities.size(), 1)));
            }
        }

        auto positions = coordinate_values(coordinates, input_graph);
        rescale_positions(positions,
                          margin,
                          std::max(width - margin, margin),
                          margin,
                          std::max(height - margin, margin));

        const auto node_ids = sorted_node_ids(input_graph);
        for (std::size_t i = 0; i < node_ids.size(); ++i) {
            coordinates[node_ids[i]] = positions[i];
        }

        if (progress_callback) {
            progress_callback(1.0);
        }
        return coordinates;
    }

    static std::vector<Coordinate> circular_centers(std::size_t count,
                                                    double width,
                                                    double height,
                                                    double margin) {
        std::vector<Coordinate> centers;
        centers.reserve(count);
        if (count == 0) {
            return centers;
        }
        if (count == 1) {
            centers.push_back(Coordinate{width * 0.5, height * 0.5});
            return centers;
        }
        const double radius = std::max(std::min(width, height) * 0.35 - margin, 10.0);
        const double tau = 6.28318530717958647692;
        for (std::size_t i = 0; i < count; ++i) {
            const double angle = tau * static_cast<double>(i) / static_cast<double>(count);
            centers.push_back(Coordinate{width * 0.5 + radius * std::cos(angle),
                                         height * 0.5 + radius * std::sin(angle)});
        }
        return centers;
    }

    static std::vector<Coordinate> grid_centers(std::size_t count,
                                                double width,
                                                double height,
                                                double margin) {
        std::vector<Coordinate> centers;
        centers.reserve(count);
        if (count == 0) {
            return centers;
        }
        const std::size_t cols = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
        const std::size_t rows = static_cast<std::size_t>(std::ceil(static_cast<double>(count) / static_cast<double>(cols)));
        const double cell_width = std::max((width - 2.0 * margin) / static_cast<double>(cols), 1.0);
        const double cell_height = std::max((height - 2.0 * margin) / static_cast<double>(rows), 1.0);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t row = i / cols;
            const std::size_t col = i % cols;
            centers.push_back(Coordinate{margin + (static_cast<double>(col) + 0.5) * cell_width,
                                         margin + (static_cast<double>(row) + 0.5) * cell_height});
        }
        return centers;
    }

    static std::vector<Coordinate> community_forceatlas_centers(
        const graph::Graph<graph::Node, graph::Edge>& input_graph,
        const AssignmentMap& explicit_assignment,
        double width,
        double height,
        double margin) {
        const auto quotient = build_quotient_graph(input_graph, explicit_assignment);

        ForceAtlas2Layout community_layout(100, width, height, margin, 10.0, 1.0, false, 1.0, true, 1.2, 2);
        const auto coordinates = quotient.graph.getNodes().empty()
            ? CoordinateMap{}
            : community_layout.compute(quotient.graph);

        std::vector<Coordinate> centers;
        centers.reserve(quotient.communities.size());
        for (std::size_t i = 0; i < quotient.communities.size(); ++i) {
            const auto node_id = graph::NodeId{static_cast<uint32_t>(i)};
            const auto it = coordinates.find(node_id);
            centers.push_back(it != coordinates.end() ? it->second : Coordinate{width * 0.5, height * 0.5});
        }
        return centers;
    }

    static QuotientGraph build_quotient_graph(
        const graph::Graph<graph::Node, graph::Edge>& input_graph,
        const std::unordered_map<graph::NodeId, graph::CommunityId>& explicit_assignment) {
        const auto assignment = normalize_assignment(input_graph, explicit_assignment);

        QuotientGraph quotient;
        quotient.communities = bucket_nodes(assignment);
        for (std::size_t i = 0; i < quotient.communities.size(); ++i) {
            const auto community_id = assignment.at(quotient.communities[i].front());
            const auto node_id = graph::NodeId{static_cast<uint32_t>(i)};
            quotient.community_to_node[community_id] = node_id;
            quotient.graph.createNode(node_id, "c" + std::to_string(i));
        }

        std::map<std::pair<graph::CommunityId, graph::CommunityId>, graph::EdgeId> quotient_edges;
        for (const auto& [edge_id, edge] : input_graph.getEdges()) {
            (void)edge_id;
            const auto src_comm = assignment.at(edge.source);
            const auto tgt_comm = assignment.at(edge.target);
            if (src_comm == tgt_comm) {
                continue;
            }
            const auto ordered = std::minmax(src_comm, tgt_comm);
            auto pair_it = quotient_edges.find(ordered);
            if (pair_it == quotient_edges.end()) {
                auto& created = quotient.graph.createEdge(quotient.community_to_node.at(ordered.first),
                                                          quotient.community_to_node.at(ordered.second));
                created.weight = edge.weight;
                quotient.graph.connect(created.id, quotient.community_to_node.at(ordered.first));
                quotient_edges.emplace(ordered, created.id);
            } else {
                quotient.graph.getEdges().at(pair_it->second).weight += edge.weight;
            }
        }

        return quotient;
    }

private:
    static AssignmentMap normalize_assignment(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                                              const AssignmentMap& explicit_assignment) {
        AssignmentMap assignment = explicit_assignment;
        for (const auto& [node_id, node] : input_graph.getNodes()) {
            (void)node;
            if (assignment.find(node_id) == assignment.end()) {
                assignment.emplace(node_id, graph::CommunityId{0});
            }
        }
        return assignment;
    }

    static std::vector<std::vector<graph::NodeId>> bucket_nodes(const AssignmentMap& assignment) {
        std::map<graph::CommunityId, std::vector<graph::NodeId>> grouped;
        for (const auto& [node_id, community_id] : assignment) {
            grouped[community_id].push_back(node_id);
        }
        std::vector<std::vector<graph::NodeId>> communities;
        communities.reserve(grouped.size());
        for (auto& [community_id, node_ids] : grouped) {
            (void)community_id;
            std::sort(node_ids.begin(), node_ids.end());
            communities.push_back(std::move(node_ids));
        }
        return communities;
    }

    static std::vector<graph::NodeId> sorted_node_ids(const graph::Graph<graph::Node, graph::Edge>& input_graph) {
        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(input_graph.getNodes().size());
        for (const auto& [node_id, node] : input_graph.getNodes()) {
            (void)node;
            node_ids.push_back(node_id);
        }
        std::sort(node_ids.begin(), node_ids.end());
        return node_ids;
    }

    static graph::Graph<graph::Node, graph::Edge> induced_subgraph(
        const graph::Graph<graph::Node, graph::Edge>& input_graph,
        const std::vector<graph::NodeId>& node_ids) {
        graph::Graph<graph::Node, graph::Edge> subgraph;
        std::unordered_map<graph::NodeId, bool> included;
        for (const auto node_id : node_ids) {
            const auto node_it = input_graph.getNodes().find(node_id);
            if (node_it == input_graph.getNodes().end()) {
                continue;
            }
            subgraph.createNode(node_id, node_it->second.name);
            included[node_id] = true;
        }
        for (const auto& [edge_id, edge] : input_graph.getEdges()) {
            (void)edge_id;
            if (included.find(edge.source) == included.end() ||
                included.find(edge.target) == included.end()) {
                continue;
            }
            auto& created = subgraph.createEdge(edge.source, edge.target);
            created.weight = edge.weight;
            subgraph.connect(created.id, edge.source);
        }
        return subgraph;
    }

    static std::vector<Coordinate> coordinate_values(CoordinateMap& coordinates,
                                                     const graph::Graph<graph::Node, graph::Edge>& input_graph) {
        std::vector<Coordinate> positions;
        const auto node_ids = sorted_node_ids(input_graph);
        positions.reserve(node_ids.size());
        for (const auto node_id : node_ids) {
            positions.push_back(coordinates[node_id]);
        }
        return positions;
    }

    static void rescale_positions(std::vector<Coordinate>& positions,
                                  double target_min_x,
                                  double target_max_x,
                                  double target_min_y,
                                  double target_max_y) {
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (const auto& position : positions) {
            min_x = std::min(min_x, position.x);
            max_x = std::max(max_x, position.x);
            min_y = std::min(min_y, position.y);
            max_y = std::max(max_y, position.y);
        }
        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double target_span_x = target_max_x - target_min_x;
        const double target_span_y = target_max_y - target_min_y;
        for (auto& position : positions) {
            position.x = (span_x <= 1e-9)
                ? (target_min_x + target_max_x) * 0.5
                : target_min_x + ((position.x - min_x) / span_x) * target_span_x;
            position.y = (span_y <= 1e-9)
                ? (target_min_y + target_max_y) * 0.5
                : target_min_y + ((position.y - min_y) / span_y) * target_span_y;
        }
    }
};

inline CircularLayout::CircularLayout(double width, double height, double margin)
    : width_(width), height_(height), margin_(margin) {}

inline void CircularLayout::setCommunityAssignment(
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment) {
    assignment_ = std::move(assignment);
}

inline CoordinateMap CircularLayout::compute(
    const graph::Graph<graph::Node, graph::Edge>& input_graph,
    ProgressCallback progress_callback) {
    return CommunityLayoutSupport::compose(
        input_graph,
        assignment_,
        [&](double local_width, double local_height) {
            return FruchtermanReingoldLayout(150, local_width, local_height, 0.0);
        },
        [&](std::size_t community_count) {
            return CommunityLayoutSupport::circular_centers(community_count, width_, height_, margin_);
        },
        width_,
        height_,
        margin_,
        progress_callback);
}

inline GridLayout::GridLayout(double width, double height, double margin)
    : width_(width), height_(height), margin_(margin) {}

inline void GridLayout::setCommunityAssignment(
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment) {
    assignment_ = std::move(assignment);
}

inline CoordinateMap GridLayout::compute(
    const graph::Graph<graph::Node, graph::Edge>& input_graph,
    ProgressCallback progress_callback) {
    return CommunityLayoutSupport::compose(
        input_graph,
        assignment_,
        [&](double local_width, double local_height) {
            return FruchtermanReingoldLayout(150, local_width, local_height, 0.0);
        },
        [&](std::size_t community_count) {
            return CommunityLayoutSupport::grid_centers(community_count, width_, height_, margin_);
        },
        width_,
        height_,
        margin_,
        progress_callback);
}

inline GridKamadaKawaiLayout::GridKamadaKawaiLayout(double width, double height, double margin)
    : width_(width), height_(height), margin_(margin) {}

inline void GridKamadaKawaiLayout::setCommunityAssignment(
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment) {
    assignment_ = std::move(assignment);
}

inline CoordinateMap GridKamadaKawaiLayout::compute(
    const graph::Graph<graph::Node, graph::Edge>& input_graph,
    ProgressCallback progress_callback) {
    return CommunityLayoutSupport::compose(
        input_graph,
        assignment_,
        [&](double local_width, double local_height) {
            return KamadaKawaiLayout(120, local_width, local_height, 0.0);
        },
        [&](std::size_t community_count) {
            return CommunityLayoutSupport::grid_centers(community_count, width_, height_, margin_);
        },
        width_,
        height_,
        margin_,
        progress_callback);
}

inline CommunityForceAtlas2Layout::CommunityForceAtlas2Layout(double width, double height, double margin)
    : width_(width), height_(height), margin_(margin) {}

inline void CommunityForceAtlas2Layout::setCommunityAssignment(
    std::unordered_map<graph::NodeId, graph::CommunityId> assignment) {
    assignment_ = std::move(assignment);
}

inline CoordinateMap CommunityForceAtlas2Layout::compute(
    const graph::Graph<graph::Node, graph::Edge>& input_graph,
    ProgressCallback progress_callback) {
    const auto centers = CommunityLayoutSupport::community_forceatlas_centers(
        input_graph, assignment_, width_, height_, margin_);
    return CommunityLayoutSupport::compose(
        input_graph,
        assignment_,
        [&](double local_width, double local_height) {
            return FruchtermanReingoldLayout(120, local_width, local_height, 0.0);
        },
        [&](std::size_t) { return centers; },
        width_,
        height_,
        margin_,
        progress_callback);
}

// ---------------------------------------------------------------------------
// GPU Fruchterman-Reingold (OpenCL)
// ---------------------------------------------------------------------------

namespace detail {

struct ClContextDeleter { void operator()(cl_context p) const { if (p) clReleaseContext(p); } };
struct ClQueueDeleter  { void operator()(cl_command_queue p) const { if (p) clReleaseCommandQueue(p); } };
struct ClProgramDeleter{ void operator()(cl_program p) const { if (p) clReleaseProgram(p); } };
struct ClKernelDeleter { void operator()(cl_kernel p) const { if (p) clReleaseKernel(p); } };
struct ClMemDeleter    { void operator()(cl_mem p) const { if (p) clReleaseMemObject(p); } };

using ClContextPtr  = std::unique_ptr<std::remove_pointer_t<cl_context>,  ClContextDeleter>;
using ClQueuePtr   = std::unique_ptr<std::remove_pointer_t<cl_command_queue>, ClQueueDeleter>;
using ClProgramPtr = std::unique_ptr<std::remove_pointer_t<cl_program>, ClProgramDeleter>;
using ClKernelPtr  = std::unique_ptr<std::remove_pointer_t<cl_kernel>,  ClKernelDeleter>;
using ClMemPtr     = std::unique_ptr<std::remove_pointer_t<cl_mem>,     ClMemDeleter>;

inline ClMemPtr make_cl_mem(cl_context ctx, cl_mem_flags flags, std::size_t size,
                             const void* host_ptr) {
    cl_int err = CL_SUCCESS;
    cl_mem m = clCreateBuffer(ctx, flags, size, const_cast<void*>(host_ptr), &err);
    if (err != CL_SUCCESS || m == nullptr) { return nullptr; }
    return ClMemPtr(m);
}

inline constexpr const char* kFrKernelSource = R"(
__kernel void fr_repel(
    __global const float* optDist,
    __global const int*   nIndexes,
    __global const float* xPos,
    __global const float* yPos,
    __global       float* xDisp,
    __global       float* yDisp,
    __global const int*   nodes,
    __global const int*   startIndexes,
    __global const int*   totalWork)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    int nNodes = nodes[0];
    int v = 0, u = 0;
    int found = 0;

    for (int i = 0; i < nNodes; i++) {
        if (found == 1) { break; }
        if (gid < startIndexes[i]) {
            v = i - 1;
            u = (gid - startIndexes[i - 1]) + 1;
            found = 1;
        }
    }

    float xDelta = xPos[v] - xPos[u];
    float yDelta = yPos[v] - yPos[u];
    float deltaLength = sqrt(xDelta * xDelta + yDelta * yDelta);
    if (deltaLength == 0.0f) {
        xDisp[gid] = 0.0f;
        yDisp[gid] = 0.0f;
        return;
    }
    float force = optDist[0] / deltaLength;
    xDisp[gid] = (xDelta / deltaLength) * force;
    yDisp[gid] = (yDelta / deltaLength) * force;
}

__kernel void fr_attract(
    __global const float* optDist,
    __global const int*   edgesStart,
    __global const int*   edgesEnd,
    __global const float* xPos,
    __global const float* yPos,
    __global       float* xDisp,
    __global       float* yDisp,
    __global const int*   totalWork)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    int v = edgesStart[gid];
    int u = edgesEnd[gid];
    float xDelta = xPos[v] - xPos[u];
    float yDelta = yPos[v] - yPos[u];
    float deltaLength = sqrt(xDelta * xDelta + yDelta * yDelta);
    if (deltaLength == 0.0f) { deltaLength = 0.001f; }
    float force = (deltaLength * deltaLength) / optDist[0];
    xDisp[gid] = (xDelta / deltaLength) * force;
    yDisp[gid] = (yDelta / deltaLength) * force;
}

__kernel void fr_attract_aggregate1(
    __global const int*   edgesStart,
    __global const int*   edgesEnd,
    __global const float* xDispIn,
    __global const float* yDispIn,
    __global       float* xDispOut,
    __global       float* yDispOut,
    __global const int*   chunks,
    __global const int*   nEdges,
    __global const int*   totalWork)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    int chunk = gid % chunks[0];
    int start = gid * chunks[0];
    int end   = min((gid + 1) * chunks[0], nEdges[0]);
    for (int i = start; i < end; i++) {
        int vs = edgesStart[i];
        int ve = edgesEnd[i];
        xDispOut[vs * chunks[0] + chunk] -= xDispIn[i];
        yDispOut[vs * chunks[0] + chunk] -= yDispIn[i];
        xDispOut[ve * chunks[0] + chunk] += xDispIn[i];
        yDispOut[ve * chunks[0] + chunk] += yDispIn[i];
    }
}

__kernel void fr_attract_aggregate2(
    __global const float* xDispIn,
    __global const float* yDispIn,
    __global       float* xDispOut,
    __global       float* yDispOut,
    __global const int*   chunks,
    __global const int*   totalWork)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    int start = gid * chunks[0];
    int end   = start + chunks[0];
    xDispOut[gid] = 0.0f;
    yDispOut[gid] = 0.0f;
    for (int i = start; i < end; i++) {
        xDispOut[gid] += xDispIn[i];
        yDispOut[gid] += yDispIn[i];
    }
}

__kernel void fr_repel_aggregate(
    __global const float* xDispIn,
    __global const float* yDispIn,
    __global       float* xDispOut,
    __global       float* yDispOut,
    __global const int*   nodes,
    __global const int*   startIndexes,
    __global const int*   totalWork)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    xDispOut[gid] = 0.0f;
    yDispOut[gid] = 0.0f;
    int idx = startIndexes[gid];
    for (int ui = gid + 1; ui < nodes[0]; ui++) {
        xDispOut[gid] += xDispIn[idx];
        yDispOut[gid] += yDispIn[idx];
        idx++;
    }
}

__kernel void fr_adjust(
    __global       float* xDisp,
    __global       float* yDisp,
    __global       float* xPos,
    __global       float* yPos,
    __global const int*   totalWork,
    __global       float* temp,
    __global       int*   passes,
    __global       int*   stop,
    __global const int*   maxPasses,
    __global const int*   coolStart)
{
    int gid = get_global_id(0);
    if (gid >= totalWork[0]) { return; }

    float xd = xDisp[gid];
    float yd = yDisp[gid];
    float dl = sqrt(xd * xd + yd * yd);
    float t  = temp[0];
    if (dl > t) {
        xPos[gid] += xd / (dl / t);
        yPos[gid] += yd / (dl / t);
    } else {
        xPos[gid] += xd;
        yPos[gid] += yd;
    }
    xDisp[gid] = 0.0f;
    yDisp[gid] = 0.0f;

    if (gid == 0) {
        passes[0]++;
        if (passes[0] > coolStart[0]) {
            temp[0] = temp[0] / 1.1f;
        }
        if (temp[0] < 1.0f) {
            stop[0] = maxPasses[0];
        } else {
            stop[0] = passes[0];
        }
    }
}
)";

} // namespace detail

class GpuFruchtermanReingoldLayout : public Layout {
public:
    explicit GpuFruchtermanReingoldLayout(std::size_t iterations = 500,
                                          double width = 1024.0,
                                          double height = 1024.0,
                                          double margin = 0.0,
                                          double k_scale = 0.46)
        : iterations_(iterations)
        , width_(width)
        , height_(height)
        , margin_(margin)
        , k_scale_(k_scale) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                          ProgressCallback progress_callback = nullptr) override {
        try {
            return compute_gpu(input_graph, progress_callback);
        } catch (const std::exception& e) {
            if (progress_callback) {
                progress_callback(0.0);
            }
            FruchtermanReingoldLayout cpu_fallback(iterations_, width_, height_, margin_, k_scale_);
            return cpu_fallback.compute(input_graph, progress_callback);
        }
    }

private:
    static constexpr int kChunks = 100;
    static constexpr int kCoolStart = 30;

    CoordinateMap compute_gpu(const graph::Graph<graph::Node, graph::Edge>& input_graph,
                              ProgressCallback progress_callback) {
        CoordinateMap coordinates;
        const auto& nodes = input_graph.getNodes();
        if (nodes.empty()) {
            if (progress_callback) { progress_callback(1.0); }
            return coordinates;
        }

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(nodes.size());
        for (const auto& [nid, node] : nodes) {
            (void)node;
            node_ids.push_back(nid);
        }
        std::sort(node_ids.begin(), node_ids.end());

        if (progress_callback) { progress_callback(0.0); }

        const int n = static_cast<int>(node_ids.size());
        if (n == 1) {
            coordinates.emplace(node_ids.front(), Coordinate{width_ * 0.5, height_ * 0.5});
            if (progress_callback) { progress_callback(1.0); }
            return coordinates;
        }

        // --- OpenCL setup ---
        auto [ctx, queue, max_wg] = init_opencl();
        if (!ctx || !queue) {
            throw std::runtime_error("OpenCL context creation failed");
        }

        // --- Compile kernels ---
        auto kernels = build_kernels(ctx.get());
        if (kernels.empty()) {
            throw std::runtime_error("OpenCL kernel compilation failed");
        }

        // --- Prepare host data ---
        const double area = width_ * height_;
        const double node_count = static_cast<double>(n);
        const double k = k_scale_ * std::sqrt(area / node_count);
        const float opt_dist = static_cast<float>(k);
        const float repel_od = static_cast<float>(std::pow(k, 4));
        const float initial_temp = static_cast<float>(width_ / 10.0);

        // Initialize positions (circular)
        std::vector<float> xPos(n), yPos(n);
        const double cx = width_ * 0.5, cy = height_ * 0.5;
        const double radius = std::max(std::min(width_, height_) * 0.35, 1.0);
        const double tau = 6.28318530717958647692;
        for (int i = 0; i < n; i++) {
            const double angle = tau * static_cast<double>(i) / static_cast<double>(n);
            xPos[i] = static_cast<float>(cx + radius * std::cos(angle));
            yPos[i] = static_cast<float>(cy + radius * std::sin(angle));
        }

        // Edge data
        const auto& edges = input_graph.getEdges();
        std::unordered_map<graph::NodeId, int> node_index;
        for (int i = 0; i < n; i++) { node_index.emplace(node_ids[i], i); }

        std::vector<int> edges_start, edges_end;
        edges_start.reserve(edges.size());
        edges_end.reserve(edges.size());
        for (const auto& [eid, edge] : edges) {
            (void)eid;
            auto si = node_index.find(edge.source);
            auto ti = node_index.find(edge.target);
            if (si != node_index.end() && ti != node_index.end()) {
                edges_start.push_back(si->second);
                edges_end.push_back(ti->second);
            }
        }
        const int nEdges = static_cast<int>(edges_start.size());

        // Repulsion pair index
        const int repel_work = (n * (n - 1)) / 2;
        std::vector<int> startIndexes(n, 0);
        for (int i = 0; i < n - 1; i++) {
            startIndexes[i + 1] = startIndexes[i] + (n - (i + 1));
        }

        // --- Allocate device buffers ---
        const auto ro = static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
        const auto rw = static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
        const auto rwb = static_cast<cl_mem_flags>(CL_MEM_READ_WRITE);

        int nNodes = n;
        int one_i = 1;
        int chunks = kChunks;
        int zero_i = 0;
        int max_passes_i = static_cast<int>(iterations_);
        int cool_start_i = kCoolStart;
        int repel_work_i = repel_work;
        int n_nodes_i = n;
        int n_edges_i = nEdges;
        int agg1_work = chunks * n;
        int agg2_work = n;
        int repel_agg_work = n;
        int adj_work = n;

        auto dNodes          = detail::make_cl_mem(ctx.get(), ro, sizeof(int) * n, startIndexes.data());
        auto dStartIndexes   = detail::make_cl_mem(ctx.get(), ro, sizeof(int) * n, startIndexes.data());
        auto dNNodes         = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &nNodes);
        auto dRepelOd        = detail::make_cl_mem(ctx.get(), ro, sizeof(float), &repel_od);
        auto dAttractOd      = detail::make_cl_mem(ctx.get(), ro, sizeof(float), &opt_dist);
        auto dRepelWork      = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &repel_work_i);
        auto dRepelAggWork   = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &repel_agg_work);
        auto dEdgesStart     = detail::make_cl_mem(ctx.get(), ro, sizeof(int) * nEdges, edges_start.data());
        auto dEdgesEnd       = detail::make_cl_mem(ctx.get(), ro, sizeof(int) * nEdges, edges_end.data());
        auto dNEdges         = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &n_edges_i);
        auto dAttractWork    = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &n_edges_i);
        auto dAgg1Work       = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &agg1_work);
        auto dAgg2Work       = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &agg2_work);
        auto dChunks         = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &chunks);
        auto dAdjWork        = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &adj_work);
        auto dMaxPasses      = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &max_passes_i);
        auto dCoolStart      = detail::make_cl_mem(ctx.get(), ro, sizeof(int), &cool_start_i);

        float temp = initial_temp;
        int passes = 0;
        int stop = 0;
        auto dTemp  = detail::make_cl_mem(ctx.get(), rw, sizeof(float), &temp);
        auto dPasses= detail::make_cl_mem(ctx.get(), rw, sizeof(int), &passes);
        auto dStop  = detail::make_cl_mem(ctx.get(), rw, sizeof(int), &stop);

        // Position buffers (persistent across iterations)
        auto dXPos = detail::make_cl_mem(ctx.get(), rw, sizeof(float) * n, xPos.data());
        auto dYPos = detail::make_cl_mem(ctx.get(), rw, sizeof(float) * n, yPos.data());

        if (!dXPos || !dYPos || !dTemp || !dPasses || !dStop) {
            throw std::runtime_error("OpenCL buffer allocation failed");
        }

        // Intermediate buffers (allocated once, reused)
        std::vector<float> zeros_repel(repel_work, 0.0f);
        std::vector<float> zeros_attract(nEdges, 0.0f);
        std::vector<float> zeros_agg1(kChunks * n, 0.0f);
        std::vector<float> zeros_disp(n, 0.0f);

        auto dRepelDispX     = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * repel_work, nullptr);
        auto dRepelDispY     = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * repel_work, nullptr);
        auto dAttractDispX   = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * nEdges, nullptr);
        auto dAttractDispY   = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * nEdges, nullptr);
        auto dAgg1X          = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * kChunks * n, nullptr);
        auto dAgg1Y          = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * kChunks * n, nullptr);
        auto dDispX          = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * n, nullptr);
        auto dDispY          = detail::make_cl_mem(ctx.get(), rwb, sizeof(float) * n, nullptr);

        if (!dRepelDispX || !dRepelDispY || !dAttractDispX || !dAttractDispY
            || !dAgg1X || !dAgg1Y || !dDispX || !dDispY) {
            throw std::runtime_error("OpenCL intermediate buffer allocation failed");
        }

        cl_int err = CL_SUCCESS;
        const std::size_t max_local = static_cast<std::size_t>(max_wg);

        auto aligned = [max_local](std::size_t count) -> std::size_t {
            if (max_local == 0) return count;
            return ((count + max_local - 1) / max_local) * max_local;
        };

        // --- Iteration loop ---
        const int maxIter = static_cast<int>(iterations_);
        for (int iter = 0; iter < maxIter; iter++) {
            // 1. Repel: compute repulsive forces for each unique pair
            set_arg(kernels[0], 0, dRepelOd.get());
            set_arg(kernels[0], 1, dNNodes.get());
            set_arg(kernels[0], 2, dXPos.get());
            set_arg(kernels[0], 3, dYPos.get());
            set_arg(kernels[0], 4, dRepelDispX.get());
            set_arg(kernels[0], 5, dRepelDispY.get());
            set_arg(kernels[0], 6, dNNodes.get());
            set_arg(kernels[0], 7, dStartIndexes.get());
            set_arg(kernels[0], 8, dRepelWork.get());
            run_kernel(queue.get(), kernels[0], aligned(repel_work), max_local);

            // 2. Repel aggregate: reduce pair results into per-node displacement
            set_arg(kernels[4], 0, dRepelDispX.get());
            set_arg(kernels[4], 1, dRepelDispY.get());
            set_arg(kernels[4], 2, dDispX.get());
            set_arg(kernels[4], 3, dDispY.get());
            set_arg(kernels[4], 4, dNNodes.get());
            set_arg(kernels[4], 5, dStartIndexes.get());
            set_arg(kernels[4], 6, dRepelAggWork.get());
            run_kernel(queue.get(), kernels[4], aligned(n), max_local);

            // 3. Attract: compute attractive forces for each edge
            set_arg(kernels[1], 0, dAttractOd.get());
            set_arg(kernels[1], 1, dEdgesStart.get());
            set_arg(kernels[1], 2, dEdgesEnd.get());
            set_arg(kernels[1], 3, dXPos.get());
            set_arg(kernels[1], 4, dYPos.get());
            set_arg(kernels[1], 5, dAttractDispX.get());
            set_arg(kernels[1], 6, dAttractDispY.get());
            set_arg(kernels[1], 7, dAttractWork.get());
            run_kernel(queue.get(), kernels[1], aligned(nEdges), max_local);

            // 4. Attract aggregate pass 1: scatter into per-node, per-chunk
            set_arg(kernels[2], 0, dEdgesStart.get());
            set_arg(kernels[2], 1, dEdgesEnd.get());
            set_arg(kernels[2], 2, dAttractDispX.get());
            set_arg(kernels[2], 3, dAttractDispY.get());
            set_arg(kernels[2], 4, dAgg1X.get());
            set_arg(kernels[2], 5, dAgg1Y.get());
            set_arg(kernels[2], 6, dChunks.get());
            set_arg(kernels[2], 7, dNEdges.get());
            set_arg(kernels[2], 8, dAgg1Work.get());
            run_kernel(queue.get(), kernels[2], aligned(kChunks * n), max_local);

            // 5. Attract aggregate pass 2: reduce chunks into per-node
            set_arg(kernels[3], 0, dAgg1X.get());
            set_arg(kernels[3], 1, dAgg1Y.get());
            set_arg(kernels[3], 2, dDispX.get());
            set_arg(kernels[3], 3, dDispY.get());
            set_arg(kernels[3], 4, dChunks.get());
            set_arg(kernels[3], 5, dAgg2Work.get());
            run_kernel(queue.get(), kernels[3], aligned(n), max_local);

            // 6. Adjust: apply displacement with temperature clamping + cooling
            set_arg(kernels[5], 0, dDispX.get());
            set_arg(kernels[5], 1, dDispY.get());
            set_arg(kernels[5], 2, dXPos.get());
            set_arg(kernels[5], 3, dYPos.get());
            set_arg(kernels[5], 4, dAdjWork.get());
            set_arg(kernels[5], 5, dTemp.get());
            set_arg(kernels[5], 6, dPasses.get());
            set_arg(kernels[5], 7, dStop.get());
            set_arg(kernels[5], 8, dMaxPasses.get());
            set_arg(kernels[5], 9, dCoolStart.get());
            run_kernel(queue.get(), kernels[5], aligned(n), max_local);

            clFinish(queue.get());

            // Check stop condition
            int stop_val = 0;
            err = clEnqueueReadBuffer(queue.get(), dStop.get(), CL_TRUE, 0,
                                       sizeof(int), &stop_val, 0, nullptr, nullptr);
            if (err != CL_SUCCESS || stop_val >= maxIter) {
                break;
            }

            if (progress_callback) {
                progress_callback(static_cast<double>(iter + 1) / static_cast<double>(maxIter));
            }
        }

        // Read back final positions
        err = clEnqueueReadBuffer(queue.get(), dXPos.get(), CL_TRUE, 0,
                                   sizeof(float) * n, xPos.data(), 0, nullptr, nullptr);
        err = clEnqueueReadBuffer(queue.get(), dYPos.get(), CL_TRUE, 0,
                                   sizeof(float) * n, yPos.data(), 0, nullptr, nullptr);

        // Rescale to bounds
        rescale_positions(xPos, yPos, n);

        for (int i = 0; i < n; i++) {
            coordinates.emplace(node_ids[i], Coordinate{
                static_cast<double>(xPos[i]),
                static_cast<double>(yPos[i])
            });
        }

        if (progress_callback) { progress_callback(1.0); }
        return coordinates;
    }

    struct ClInitResult {
        detail::ClContextPtr ctx;
        detail::ClQueuePtr  queue;
        std::size_t max_workgroup_size;
    };

    static ClInitResult init_opencl() {
        cl_uint num_platforms = 0;
        clGetPlatformIDs(0, nullptr, &num_platforms);
        if (num_platforms == 0) {
            return {nullptr, nullptr, 0};
        }

        std::vector<cl_platform_id> platforms(num_platforms);
        clGetPlatformIDs(num_platforms, platforms.data(), nullptr);

        // Prefer AMD platform, fall back to first available
        cl_platform_id platform = platforms[0];
        for (cl_uint i = 0; i < num_platforms; i++) {
            char vendor[256] = {};
            clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, sizeof(vendor), vendor, nullptr);
            if (std::string(vendor).find("AMD") != std::string::npos
                || std::string(vendor).find("Advanced Micro Devices") != std::string::npos) {
                platform = platforms[i];
                break;
            }
        }

        cl_uint num_devices = 0;
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);
        if (num_devices == 0) {
            return {nullptr, nullptr, 0};
        }

        std::vector<cl_device_id> devices(num_devices);
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, num_devices, devices.data(), nullptr);
        cl_device_id device = devices[0];

        cl_int err = CL_SUCCESS;
        cl_context_properties props[] = {CL_CONTEXT_PLATFORM,
                                          reinterpret_cast<cl_context_properties>(platform), 0};
        cl_context raw_ctx = clCreateContext(props, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS || !raw_ctx) {
            return {nullptr, nullptr, 0};
        }
        detail::ClContextPtr ctx(raw_ctx);

        cl_command_queue raw_q = clCreateCommandQueue(ctx.get(), device, 0, &err);
        if (err != CL_SUCCESS || !raw_q) {
            return {nullptr, nullptr, 0};
        }
        detail::ClQueuePtr queue(raw_q);

        std::size_t max_wg = 256;
        clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(std::size_t), &max_wg, nullptr);

        return {std::move(ctx), std::move(queue), max_wg};
    }

    static std::vector<cl_kernel> build_kernels(cl_context ctx) {
        const char* src = detail::kFrKernelSource;
        const std::size_t len = std::char_traits<char>::length(src);
        cl_int err = CL_SUCCESS;

        cl_program prog = clCreateProgramWithSource(ctx, 1, &src, &len, &err);
        if (err != CL_SUCCESS || !prog) { return {}; }

        err = clBuildProgram(prog, 0, nullptr, nullptr, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            clReleaseProgram(prog);
            return {};
        }

        const char* names[] = {
            "fr_repel",
            "fr_attract",
            "fr_attract_aggregate1",
            "fr_attract_aggregate2",
            "fr_repel_aggregate",
            "fr_adjust"
        };

        std::vector<cl_kernel> kernels;
        kernels.reserve(6);
        for (const char* name : names) {
            cl_kernel k = clCreateKernel(prog, name, &err);
            if (err != CL_SUCCESS || !k) {
                for (auto& ki : kernels) { clReleaseKernel(ki); }
                clReleaseProgram(prog);
                return {};
            }
            kernels.push_back(k);
        }
        clReleaseProgram(prog);
        return kernels;
    }

    static void set_arg(cl_kernel kernel, cl_uint idx, cl_mem mem) {
        clSetKernelArg(kernel, idx, sizeof(cl_mem), &mem);
    }

    static void run_kernel(cl_command_queue queue, cl_kernel kernel,
                           std::size_t global, std::size_t local) {
        if (global == 0) return;
        if (local > 0 && global >= local) {
            clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global, &local, 0, nullptr, nullptr);
        } else {
            clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global, nullptr, 0, nullptr, nullptr);
        }
    }

    void rescale_positions(std::vector<float>& xPos, std::vector<float>& yPos, int n) const {
        float min_x = xPos[0], max_x = xPos[0];
        float min_y = yPos[0], max_y = yPos[0];
        for (int i = 1; i < n; i++) {
            min_x = std::min(min_x, xPos[i]); max_x = std::max(max_x, xPos[i]);
            min_y = std::min(min_y, yPos[i]); max_y = std::max(max_y, yPos[i]);
        }
        const float span_x = max_x - min_x;
        const float span_y = max_y - min_y;
        const float target_x = static_cast<float>(std::max(width_ - 2.0 * margin_, 1.0));
        const float target_y = static_cast<float>(std::max(height_ - 2.0 * margin_, 1.0));
        const float mx = static_cast<float>(margin_);
        const float my = static_cast<float>(margin_);

        for (int i = 0; i < n; i++) {
            xPos[i] = (span_x < 1e-6f) ? mx + target_x * 0.5f
                     : mx + ((xPos[i] - min_x) / span_x) * target_x;
            yPos[i] = (span_y < 1e-6f) ? my + target_y * 0.5f
                     : my + ((yPos[i] - min_y) / span_y) * target_y;
        }
    }

    std::size_t iterations_;
    double width_;
    double height_;
    double margin_;
    double k_scale_;
};

// ---------------------------------------------------------------------------
// Community-Weighted Layout (2D)
// ---------------------------------------------------------------------------

class CommunityWeightedLayout : public Layout {
    int iterations_;
    double width_;
    double height_;

    static constexpr double margin_ratio = 0.08;

public:
    explicit CommunityWeightedLayout(int iterations = 50, double width = 1024.0, double height = 1024.0)
        : iterations_(iterations), width_(width), height_(height) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& graph,
                          ProgressCallback progress = nullptr) override {
        CoordinateMap coords;
        if (graph.getNodes().empty()) {
            if (progress) progress(1.0);
            return coords;
        }

        const auto num_nodes = static_cast<double>(graph.getNodes().size());
        const double margin = std::min(width_, height_) * margin_ratio;
        const double cx = width_ * 0.5;
        const double cy = height_ * 0.5;

        // Step 1: Initialize positions on a circle centered at canvas center
        const double circle_radius = std::min(width_, height_) * 0.35;
        const double angle_step = 2.0 * M_PI / num_nodes;
        {
            double angle = 0.0;
            for (const auto& [nid, node] : graph.getNodes()) {
                (void)node;
                coords[nid] = Coordinate{
                    cx + circle_radius * std::cos(angle),
                    cy + circle_radius * std::sin(angle)
                };
                angle += angle_step;
            }
        }

        // Step 2: Compute ideal spring length
        const double area = width_ * height_;
        const double k = std::sqrt(area / num_nodes);

        // Pre-collect node IDs for iteration
        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(graph.getNodes().size());
        for (const auto& [nid, node] : graph.getNodes()) {
            (void)node;
            node_ids.push_back(nid);
        }

        const double max_temp = k * 0.5;

        for (int iter = 0; iter < iterations_; ++iter) {
            CoordinateMap displacements;
            for (const auto& nid : node_ids) {
                displacements[nid] = Coordinate{0.0, 0.0};
            }

            // Step 3a: Repulsion (all pairs)
            for (std::size_t i = 0; i < node_ids.size(); ++i) {
                for (std::size_t j = i + 1; j < node_ids.size(); ++j) {
                    const auto& nid1 = node_ids[i];
                    const auto& nid2 = node_ids[j];
                    double dx = coords[nid1].x - coords[nid2].x;
                    double dy = coords[nid1].y - coords[nid2].y;
                    double dist_sq = dx * dx + dy * dy;
                    if (dist_sq < 1.0) dist_sq = 1.0;
                    const double dist = std::sqrt(dist_sq);
                    const double force = (k * k * k) / dist_sq;
                    const double fx = (dx / dist) * force;
                    const double fy = (dy / dist) * force;
                    displacements[nid1].x += fx;
                    displacements[nid1].y += fy;
                    displacements[nid2].x -= fx;
                    displacements[nid2].y -= fy;
                }
            }

            // Step 3b: Attraction (connected pairs, weight-based)
            for (const auto& [eid, edge] : graph.getEdges()) {
                (void)eid;
                double dx = coords[edge.source].x - coords[edge.target].x;
                double dy = coords[edge.source].y - coords[edge.target].y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 1.0) dist = 1.0;
                const double weight = edge.weight > 0.0 ? edge.weight : 1.0;
                // Higher weight → shorter ideal distance
                const double ideal_dist = k / (1.0 + std::log(weight));
                // Spring force: proportional to displacement from ideal
                const double force = (dist - ideal_dist) * (dist - ideal_dist) / k * weight;
                displacements[edge.source].x -= (dx / dist) * force;
                displacements[edge.source].y -= (dy / dist) * force;
                displacements[edge.target].x += (dx / dist) * force;
                displacements[edge.target].y += (dy / dist) * force;
            }

            // Step 3c: Gravity toward center
            for (const auto& nid : node_ids) {
                double dx = cx - coords[nid].x;
                double dy = cy - coords[nid].y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.0) {
                    const double gravity_force = dist * 0.01;
                    displacements[nid].x += (dx / dist) * gravity_force;
                    displacements[nid].y += (dy / dist) * gravity_force;
                }
            }

            // Step 3d: Apply with quadratic cooling
            const double t_ratio = static_cast<double>(iter) / static_cast<double>(iterations_);
            const double temperature = max_temp * (1.0 - t_ratio) * (1.0 - t_ratio);
            for (const auto& nid : node_ids) {
                double d = std::sqrt(displacements[nid].x * displacements[nid].x +
                                      displacements[nid].y * displacements[nid].y);
                if (d > 0.0) {
                    const double scale = std::min(d, temperature) / d;
                    coords[nid].x += displacements[nid].x * scale;
                    coords[nid].y += displacements[nid].y * scale;
                    // Clamp to bounds with margin
                    coords[nid].x = std::max(margin, std::min(width_ - margin, coords[nid].x));
                    coords[nid].y = std::max(margin, std::min(height_ - margin, coords[nid].y));
                }
            }

            if (progress) {
                const double p = static_cast<double>(iter + 1) / static_cast<double>(iterations_);
                progress(p);
            }
        }

        // Step 4: Final normalization — scale positions to fill canvas
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (const auto& [nid, c] : coords) {
            (void)nid;
            min_x = std::min(min_x, c.x);
            max_x = std::max(max_x, c.x);
            min_y = std::min(min_y, c.y);
            max_y = std::max(max_y, c.y);
        }
        const double span_x = std::max(max_x - min_x, 1.0);
        const double span_y = std::max(max_y - min_y, 1.0);
        const double scale = std::min((width_ - 2.0 * margin) / span_x,
                                       (height_ - 2.0 * margin) / span_y);
        const double new_cx = (min_x + max_x) * 0.5;
        const double new_cy = (min_y + max_y) * 0.5;
        for (auto& [nid, c] : coords) {
            (void)nid;
            c.x = cx + (c.x - new_cx) * scale;
            c.y = cy + (c.y - new_cy) * scale;
            c.x = std::max(margin, std::min(width_ - margin, c.x));
            c.y = std::max(margin, std::min(height_ - margin, c.y));
        }

        return coords;
    }
};

// ---------------------------------------------------------------------------
// Community-Weighted Layout (3D)
// ---------------------------------------------------------------------------

class CommunityWeighted3DLayout : public Layout {
    int iterations_;
    double w_;
    double h_;
    double d_;

    static constexpr double margin_ratio = 0.08;

public:
    explicit CommunityWeighted3DLayout(int iterations = 50,
                                       double w = 1024.0,
                                       double h = 1024.0,
                                       double d = 1024.0)
        : iterations_(iterations), w_(w), h_(h), d_(d) {}

    CoordinateMap compute(const graph::Graph<graph::Node, graph::Edge>& graph,
                          ProgressCallback progress = nullptr) override {
        auto coords3d = compute3D(graph, progress);
        CoordinateMap coords2d;
        for (const auto& [id, c3d] : coords3d) {
            coords2d.emplace(id, Coordinate{c3d.x, c3d.y});
        }
        return coords2d;
    }

    Coordinate3DMap compute3D(const graph::Graph<graph::Node, graph::Edge>& graph,
                               ProgressCallback progress = nullptr) {
        Coordinate3DMap coords;
        if (graph.getNodes().empty()) {
            if (progress) progress(1.0);
            return coords;
        }

        const auto num_nodes = static_cast<double>(graph.getNodes().size());
        const double margin = std::min({w_, h_, d_}) * margin_ratio;
        const double cx = w_ * 0.5;
        const double cy = h_ * 0.5;
        const double cz = d_ * 0.5;

        // Step 1: Initialize on a sphere centered at cube center
        const double sphere_radius = std::min({w_, h_, d_}) * 0.35;
        const double angle_step = 2.0 * M_PI / num_nodes;
        const double z_step = M_PI / (num_nodes + 1.0);
        {
            int idx = 0;
            for (const auto& [nid, node] : graph.getNodes()) {
                (void)node;
                const double phi = z_step * static_cast<double>(idx + 1);
                const double theta = angle_step * static_cast<double>(idx);
                coords[nid] = Coordinate3D{
                    cx + sphere_radius * std::sin(phi) * std::cos(theta),
                    cy + sphere_radius * std::sin(phi) * std::sin(theta),
                    cz + sphere_radius * std::cos(phi)
                };
                ++idx;
            }
        }

        // Step 2: Compute ideal spring length using volume
        const double volume = w_ * h_ * d_;
        const double k = std::cbrt(volume / num_nodes);

        std::vector<graph::NodeId> node_ids;
        node_ids.reserve(graph.getNodes().size());
        for (const auto& [nid, node] : graph.getNodes()) {
            (void)node;
            node_ids.push_back(nid);
        }

        const double max_temp = k * 0.5;

        for (int iter = 0; iter < iterations_; ++iter) {
            Coordinate3DMap displacements;
            for (const auto& nid : node_ids) {
                displacements[nid] = Coordinate3D{0.0, 0.0, 0.0};
            }

            // Step 3a: Repulsion (all pairs, 3D)
            for (std::size_t i = 0; i < node_ids.size(); ++i) {
                for (std::size_t j = i + 1; j < node_ids.size(); ++j) {
                    const auto& nid1 = node_ids[i];
                    const auto& nid2 = node_ids[j];
                    double dx = coords[nid1].x - coords[nid2].x;
                    double dy = coords[nid1].y - coords[nid2].y;
                    double dz = coords[nid1].z - coords[nid2].z;
                    double dist_sq = dx * dx + dy * dy + dz * dz;
                    if (dist_sq < 1.0) dist_sq = 1.0;
                    const double dist = std::sqrt(dist_sq);
                    const double force = (k * k * k) / dist_sq;
                    const double fx = (dx / dist) * force;
                    const double fy = (dy / dist) * force;
                    const double fz = (dz / dist) * force;
                    displacements[nid1].x += fx;
                    displacements[nid1].y += fy;
                    displacements[nid1].z += fz;
                    displacements[nid2].x -= fx;
                    displacements[nid2].y -= fy;
                    displacements[nid2].z -= fz;
                }
            }

            // Step 3b: Attraction (connected pairs, weight-based, 3D)
            for (const auto& [eid, edge] : graph.getEdges()) {
                (void)eid;
                double dx = coords[edge.source].x - coords[edge.target].x;
                double dy = coords[edge.source].y - coords[edge.target].y;
                double dz = coords[edge.source].z - coords[edge.target].z;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist < 1.0) dist = 1.0;
                const double weight = edge.weight > 0.0 ? edge.weight : 1.0;
                // Higher weight → shorter ideal distance
                const double ideal_dist = k / (1.0 + std::log(weight));
                // Spring force: proportional to displacement from ideal
                const double force = (dist - ideal_dist) * (dist - ideal_dist) / k * weight;
                displacements[edge.source].x -= (dx / dist) * force;
                displacements[edge.source].y -= (dy / dist) * force;
                displacements[edge.source].z -= (dz / dist) * force;
                displacements[edge.target].x += (dx / dist) * force;
                displacements[edge.target].y += (dy / dist) * force;
                displacements[edge.target].z += (dz / dist) * force;
            }

            // Step 3c: Gravity toward center
            for (const auto& nid : node_ids) {
                double dx = cx - coords[nid].x;
                double dy = cy - coords[nid].y;
                double dz = cz - coords[nid].z;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > 0.0) {
                    const double gravity_force = dist * 0.01;
                    displacements[nid].x += (dx / dist) * gravity_force;
                    displacements[nid].y += (dy / dist) * gravity_force;
                    displacements[nid].z += (dz / dist) * gravity_force;
                }
            }

            // Step 3d: Apply with quadratic cooling
            const double t_ratio = static_cast<double>(iter) / static_cast<double>(iterations_);
            const double temperature = max_temp * (1.0 - t_ratio) * (1.0 - t_ratio);
            for (const auto& nid : node_ids) {
                double mag = std::sqrt(displacements[nid].x * displacements[nid].x +
                                        displacements[nid].y * displacements[nid].y +
                                        displacements[nid].z * displacements[nid].z);
                if (mag > 0.0) {
                    const double scale = std::min(mag, temperature) / mag;
                    coords[nid].x += displacements[nid].x * scale;
                    coords[nid].y += displacements[nid].y * scale;
                    coords[nid].z += displacements[nid].z * scale;
                    coords[nid].x = std::max(margin, std::min(w_ - margin, coords[nid].x));
                    coords[nid].y = std::max(margin, std::min(h_ - margin, coords[nid].y));
                    coords[nid].z = std::max(margin, std::min(d_ - margin, coords[nid].z));
                }
            }

            if (progress) {
                const double p = static_cast<double>(iter + 1) / static_cast<double>(iterations_);
                progress(p);
            }
        }

        // Step 4: Final normalization — scale to fill 3D space
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();
        for (const auto& [nid, c] : coords) {
            (void)nid;
            min_x = std::min(min_x, c.x);
            max_x = std::max(max_x, c.x);
            min_y = std::min(min_y, c.y);
            max_y = std::max(max_y, c.y);
            min_z = std::min(min_z, c.z);
            max_z = std::max(max_z, c.z);
        }
        const double span_x = std::max(max_x - min_x, 1.0);
        const double span_y = std::max(max_y - min_y, 1.0);
        const double span_z = std::max(max_z - min_z, 1.0);
        const double scale = std::min({(w_ - 2.0 * margin) / span_x,
                                        (h_ - 2.0 * margin) / span_y,
                                        (d_ - 2.0 * margin) / span_z});
        const double new_cx = (min_x + max_x) * 0.5;
        const double new_cy = (min_y + max_y) * 0.5;
        const double new_cz = (min_z + max_z) * 0.5;
        for (auto& [nid, c] : coords) {
            (void)nid;
            c.x = cx + (c.x - new_cx) * scale;
            c.y = cy + (c.y - new_cy) * scale;
            c.z = cz + (c.z - new_cz) * scale;
            c.x = std::max(margin, std::min(w_ - margin, c.x));
            c.y = std::max(margin, std::min(h_ - margin, c.y));
            c.z = std::max(margin, std::min(d_ - margin, c.z));
        }

        return coords;
    }
};

// ---------------------------------------------------------------------------
// Layout Mode & Factory
// ---------------------------------------------------------------------------

enum class LayoutMode { Detailed2D, Simple2D, Mode3D, Simple3D };

class LayoutFactory {
public:
    using Creator = std::function<std::unique_ptr<Layout>()>;

    struct LayoutEntry {
        Creator creator;
        std::set<LayoutMode> modes;
    };

    static LayoutFactory& instance() {
        static LayoutFactory factory;
        return factory;
    }

    void register_layout(const std::string& name, Creator creator, std::set<LayoutMode> modes = {}) {
        const std::lock_guard<std::mutex> lock(mutex_);
        entries_[name] = LayoutEntry{std::move(creator), std::move(modes)};
    }

    std::unique_ptr<Layout> create(const std::string& name) const {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(name);
        if (it == entries_.end()) {
            throw std::runtime_error("Unknown layout: '" + name + "'");
        }
        return it->second.creator();
    }

    std::vector<std::string> available_algorithms() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(entries_.size());
        for (const auto& [name, entry] : entries_) {
            (void)entry;
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> available_algorithms(LayoutMode mode) const {
        const std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, entry] : entries_) {
            if (entry.modes.empty() || entry.modes.count(mode)) {
                names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

private:
    LayoutFactory() {
        entries_["f"] = LayoutEntry{
            [] { return std::make_unique<FruchtermanReingoldLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["fgpu"] = LayoutEntry{
            [] { return std::make_unique<GpuFruchtermanReingoldLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["forceAtlas2"] = LayoutEntry{
            [] { return std::make_unique<ForceAtlas2Layout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["kk"] = LayoutEntry{
            [] { return std::make_unique<KamadaKawaiLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["c"] = LayoutEntry{
            [] { return std::make_unique<CircularLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["grid"] = LayoutEntry{
            [] { return std::make_unique<GridLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["gkk"] = LayoutEntry{
            [] { return std::make_unique<GridKamadaKawaiLayout>(); },
            {LayoutMode::Detailed2D}
        };
        entries_["fa3d"] = LayoutEntry{
            [] { return std::make_unique<ForceAtlas3DLayout>(); },
            {LayoutMode::Mode3D}
        };
        entries_["community-fa"] = LayoutEntry{
            [] { return std::make_unique<CommunityWeightedLayout>(); },
            {LayoutMode::Simple2D}
        };
        entries_["community-fa3d"] = LayoutEntry{
            [] { return std::make_unique<CommunityWeighted3DLayout>(); },
            {LayoutMode::Simple3D}
        };
    }

    mutable std::mutex mutex_;
    std::map<std::string, LayoutEntry> entries_;
};

}
