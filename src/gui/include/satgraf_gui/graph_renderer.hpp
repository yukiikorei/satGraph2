#pragma once

#include <satgraf/graph.hpp>
#include <satgraf/node.hpp>
#include <satgraf/edge.hpp>
#include <satgraf/layout.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/evolution.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFont>
#include <QApplication>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace satgraf::rendering {

// ---------------------------------------------------------------------------
// 9.2 — Color palette
// ---------------------------------------------------------------------------

inline QColor community_color(uint32_t community_id) {
    static const QColor palette[] = {
        {230, 25, 75},
        {60, 180, 75},
        {255, 225, 25},
        {0, 130, 200},
        {245, 130, 48},
        {145, 30, 180},
        {70, 240, 240},
        {240, 50, 230},
        {210, 245, 60},
        {250, 190, 212},
        {0, 128, 128},
        {220, 190, 255},
        {170, 110, 40},
        {255, 250, 200},
        {128, 0, 0},
        {170, 255, 195},
        {128, 128, 0},
        {255, 215, 180},
        {0, 0, 128},
        {128, 128, 128},
    };
    constexpr size_t n = sizeof(palette) / sizeof(palette[0]);
    return palette[community_id % n];
}

inline QColor inter_community_color() {
    return QColor(180, 180, 180);
}

inline QColor conflict_edge_color() {
    return QColor(255, 0, 0);
}

// ---------------------------------------------------------------------------
// 9.10 — Visibility filters
// ---------------------------------------------------------------------------

struct VisibilityFilters {
    std::unordered_set<uint32_t> visible_communities;
    bool show_unassigned{true};
    bool show_assigned_true{true};
    bool show_assigned_false{true};
    bool show_normal_edges{true};
    bool show_conflict_edges{true};

    bool is_node_visible(const graph::Node& node, uint32_t community_id) const {
        if (!visible_communities.empty() &&
            visible_communities.find(community_id) == visible_communities.end()) {
            return false;
        }
        switch (node.assignment) {
            case graph::Assignment::Unassigned: return show_unassigned;
            case graph::Assignment::True:       return show_assigned_true;
            case graph::Assignment::False:      return show_assigned_false;
        }
        return true;
    }

    bool is_edge_visible(graph::EdgeType type) const {
        return (type == graph::EdgeType::Normal) ? show_normal_edges
                                                  : show_conflict_edges;
    }
};

// ---------------------------------------------------------------------------
// 9.1 — Node graphics item
// ---------------------------------------------------------------------------

class NodeGraphicsItem : public QGraphicsEllipseItem {
public:
    NodeGraphicsItem(graph::NodeId id, const QPointF& pos, double diameter,
                     const QColor& color, uint32_t community_id)
        : QGraphicsEllipseItem(-diameter / 2, -diameter / 2, diameter, diameter),
          node_id_(id), community_id_(community_id), diameter_(diameter)
    {
        setPos(pos);
        setBrush(QBrush(color));
        setPen(QPen(color.darker(130), 1));
        setZValue(1.0);
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
    }

    graph::NodeId node_id() const { return node_id_; }
    uint32_t community_id() const { return community_id_; }
    double diameter() const { return diameter_; }

    void set_community_color(const QColor& color) {
        setBrush(QBrush(color));
        setPen(QPen(color.darker(130), 1));
    }

private:
    graph::NodeId node_id_;
    uint32_t community_id_;
    double diameter_;
};

// ---------------------------------------------------------------------------
// 9.3 — Edge graphics item
// ---------------------------------------------------------------------------

class EdgeGraphicsItem : public QGraphicsLineItem {
public:
    EdgeGraphicsItem(graph::EdgeId id, const QPointF& from, const QPointF& to,
                     const QColor& color, double width = 1.0,
                     graph::EdgeType type = graph::EdgeType::Normal)
        : QGraphicsLineItem(from.x(), from.y(), to.x(), to.y()),
          edge_id_(id), edge_type_(type)
    {
        QPen pen(QBrush(color), width);
        if (type == graph::EdgeType::Conflict) {
            pen.setStyle(Qt::DashLine);
        }
        setPen(pen);
        setZValue(0.0);
    }

    graph::EdgeId edge_id() const { return edge_id_; }
    graph::EdgeType edge_type() const { return edge_type_; }

private:
    graph::EdgeId edge_id_;
    graph::EdgeType edge_type_;
};

// ---------------------------------------------------------------------------
// 9.4 — Decision variable highlight
// ---------------------------------------------------------------------------

class DecisionHighlightItem : public QGraphicsEllipseItem {
public:
    DecisionHighlightItem(const QPointF& pos, double diameter)
        : QGraphicsEllipseItem(-diameter * 0.75, -diameter * 0.75,
                               diameter * 1.5, diameter * 1.5)
    {
        setPos(pos);
        setPen(QPen(QColor(255, 255, 0), 3));
        setBrush(Qt::NoBrush);
        setZValue(3.0);
    }
};

// ---------------------------------------------------------------------------
// 9.11 — Community region highlight
// ---------------------------------------------------------------------------

class CommunityRegionItem : public QGraphicsRectItem {
public:
    CommunityRegionItem(uint32_t community_id, const QRectF& bounds,
                        const QColor& color)
        : QGraphicsRectItem(bounds), community_id_(community_id)
    {
        QColor fill = color;
        fill.setAlpha(30);
        setBrush(QBrush(fill));
        QPen outline(color);
        outline.setWidth(2);
        outline.setStyle(Qt::DashLine);
        setPen(outline);
        setZValue(0.5);

        auto* label = new QGraphicsSimpleTextItem(
            QString::number(community_id), this);
        label->setPos(bounds.topLeft() + QPointF(4, 2));
        label->setZValue(0.6);
        QFont f;
        f.setPointSize(10);
        f.setBold(true);
        label->setFont(f);
        QColor text_color = color;
        text_color.setAlpha(180);
        label->setBrush(text_color);
    }

    uint32_t community_id() const { return community_id_; }

private:
    uint32_t community_id_;
};

// ---------------------------------------------------------------------------
// 8.4 — Community ellipse for Simple 2D click detection
// ---------------------------------------------------------------------------

class CommunityEllipseItem : public QGraphicsEllipseItem {
public:
    CommunityEllipseItem(uint32_t community_id, const QPointF& center,
                         double diameter, const QColor& color,
                         std::function<void(uint32_t)> callback)
        : QGraphicsEllipseItem(center.x() - diameter / 2.0,
                               center.y() - diameter / 2.0,
                               diameter, diameter),
          community_id_(community_id),
          click_callback_(std::move(callback))
    {
        QColor fill = color;
        fill.setAlpha(180);
        setBrush(QBrush(fill));
        setPen(QPen(color.darker(130), 1.5));
        setZValue(1.0);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setData(0, QVariant(QString("community:%1").arg(community_id)));
    }

    uint32_t community_id() const { return community_id_; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && click_callback_) {
            click_callback_(community_id_);
        }
        QGraphicsEllipseItem::mousePressEvent(event);
    }

private:
    uint32_t community_id_;
    std::function<void(uint32_t)> click_callback_;
};

// ---------------------------------------------------------------------------
// 9.7 — Zoom/pan view
// ---------------------------------------------------------------------------

class GraphView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent)
    {
        setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }

    double zoom_level() const { return zoom_level_; }

    void fit_all() {
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
        zoom_level_ = transform().m11();
        emit zoom_changed(zoom_level_);
    }

    void zoom_in(double factor = 1.2) {
        scale(factor, factor);
        zoom_level_ *= factor;
        emit zoom_changed(zoom_level_);
    }

    void zoom_out(double factor = 1.2) {
        scale(1.0 / factor, 1.0 / factor);
        zoom_level_ /= factor;
        emit zoom_changed(zoom_level_);
    }

signals:
    void node_clicked(graph::NodeId id);
    void background_clicked();
    void zoom_changed(double level);

protected:
    void wheelEvent(QWheelEvent* event) override {
        double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        scale(factor, factor);
        zoom_level_ *= factor;
        emit zoom_changed(zoom_level_);
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            QPointF scene_pos = mapToScene(event->pos());
            QGraphicsItem* item = scene()->itemAt(scene_pos, transform());
            auto* node_item = dynamic_cast<NodeGraphicsItem*>(item);
            if (node_item) {
                emit node_clicked(node_item->node_id());
                QGraphicsView::mousePressEvent(event);
                return;
            }
            emit background_clicked();
        }
        QGraphicsView::mousePressEvent(event);
    }

private:
    double zoom_level_{1.0};
};

// ---------------------------------------------------------------------------
// 9.1 — GraphRenderer (main renderer class)
// ---------------------------------------------------------------------------

class GraphRenderer : public QObject {
    Q_OBJECT
public:
    using Graph = satgraf::graph::Graph<satgraf::graph::Node, satgraf::graph::Edge>;

    explicit GraphRenderer(QObject* parent = nullptr)
        : QObject(parent)
    {
        scene_ = new QGraphicsScene(this);
    }

    QGraphicsScene* scene() const { return scene_; }

    void render(const Graph& g,
                const satgraf::layout::CoordinateMap& coords,
                const satgraf::community::CommunityResult& communities,
                double node_diameter = 10.0,
                double edge_width = 1.0)
    {
        scene_->clear();
        node_items_.clear();
        edge_items_.clear();
        community_regions_.clear();
        decision_highlight_ = nullptr;

        render_edges(g, coords, communities, edge_width);
        render_nodes(g, coords, communities, node_diameter);
        render_community_regions(coords, communities);
    }

    void render_nodes(const Graph& g,
                      const satgraf::layout::CoordinateMap& coords,
                      const satgraf::community::CommunityResult& communities,
                      double diameter)
    {
        for (const auto& [nid, node] : g.getNodes()) {
            auto it = coords.find(nid);
            if (it == coords.end()) continue;

            QPointF pos(it->second.x, it->second.y);
            uint32_t cid = 0;
            auto cit = communities.assignment.find(nid);
            if (cit != communities.assignment.end()) {
                cid = cit->second.value;
            }

            auto* item = new NodeGraphicsItem(nid, pos, diameter,
                                              community_color(cid), cid);
            scene_->addItem(item);
            node_items_[nid] = item;
        }
    }

    void render_edges(const Graph& g,
                      const satgraf::layout::CoordinateMap& coords,
                      const satgraf::community::CommunityResult& communities,
                      double width)
    {
        for (const auto& [eid, edge] : g.getEdges()) {
            auto src_it = coords.find(edge.source);
            auto tgt_it = coords.find(edge.target);
            if (src_it == coords.end() || tgt_it == coords.end()) continue;

            QPointF from(src_it->second.x, src_it->second.y);
            QPointF to(tgt_it->second.x, tgt_it->second.y);

            auto src_cit = communities.assignment.find(edge.source);
            auto tgt_cit = communities.assignment.find(edge.target);

            QColor color;
            if (edge.type == satgraf::graph::EdgeType::Conflict) {
                color = conflict_edge_color();
            } else if (src_cit != communities.assignment.end() &&
                       tgt_cit != communities.assignment.end() &&
                       src_cit->second == tgt_cit->second) {
                color = community_color(src_cit->second.value);
            } else {
                color = inter_community_color();
            }

            auto* item = new EdgeGraphicsItem(eid, from, to, color, width,
                                              edge.type);
            scene_->addItem(item);
            edge_items_[eid] = item;
        }
    }

    void set_decision_variable(std::optional<satgraf::graph::NodeId> var,
                               const satgraf::layout::CoordinateMap& coords,
                               double diameter)
    {
        if (decision_highlight_) {
            scene_->removeItem(decision_highlight_);
            delete decision_highlight_;
            decision_highlight_ = nullptr;
        }

        if (var && node_items_.count(*var)) {
            auto* node = node_items_.at(*var);
            auto* highlight = new DecisionHighlightItem(node->pos(), diameter);
            scene_->addItem(highlight);
            decision_highlight_ = highlight;
        }
    }

    std::optional<satgraf::graph::NodeId> node_at(const QPointF& scene_pos) const {
        QGraphicsItem* item = scene_->itemAt(scene_pos, QTransform());
        auto* node_item = dynamic_cast<NodeGraphicsItem*>(item);
        if (node_item) return node_item->node_id();
        return std::nullopt;
    }

    void apply_filters(const VisibilityFilters& filters,
                       const satgraf::community::CommunityResult& communities)
    {
        for (auto& [nid, item] : node_items_) {
            auto cit = communities.assignment.find(nid);
            uint32_t cid = (cit != communities.assignment.end())
                               ? cit->second.value : 0;

            auto node_opt = stored_graph_
                ? stored_graph_->getNode(nid)
                : std::optional<std::reference_wrapper<satgraf::graph::Node>>{};
            if (node_opt) {
                item->setVisible(filters.is_node_visible(node_opt->get(), cid));
            }
        }

        for (auto& [eid, item] : edge_items_) {
            item->setVisible(filters.is_edge_visible(item->edge_type()));
        }
    }

    void render_community_regions(const satgraf::layout::CoordinateMap& coords,
                                  const satgraf::community::CommunityResult& communities)
    {
        for (auto& [cid, region] : community_regions_) {
            scene_->removeItem(region);
            delete region;
        }
        community_regions_.clear();

        std::unordered_map<uint32_t, std::pair<QPointF, QPointF>> bounds;
        for (const auto& [nid, item] : node_items_) {
            auto cit = communities.assignment.find(nid);
            if (cit == communities.assignment.end()) continue;
            uint32_t cid = cit->second.value;

            QPointF p = item->pos();
            auto& [min_p, max_p] = bounds[cid];
            if (min_p.isNull()) {
                min_p = max_p = p;
            } else {
                min_p.setX(std::min(min_p.x(), p.x()));
                min_p.setY(std::min(min_p.y(), p.y()));
                max_p.setX(std::max(max_p.x(), p.x()));
                max_p.setY(std::max(max_p.y(), p.y()));
            }
        }

        for (auto& [cid, pair] : bounds) {
            auto& [min_p, max_p] = pair;
            double pad = 20.0;
            QRectF rect(min_p.x() - pad, min_p.y() - pad,
                       max_p.x() - min_p.x() + 2 * pad,
                       max_p.y() - min_p.y() + 2 * pad);
            auto* region = new CommunityRegionItem(cid, rect,
                                                   community_color(cid));
            scene_->addItem(region);
            community_regions_[cid] = region;
        }
    }

    void set_label_visibility(double zoom_level, double threshold = 2.0) {
        bool show = zoom_level >= threshold;
        for (auto& [nid, item] : node_items_) {
            auto* label = item->childItems().value(0);
            if (label) label->setVisible(show);
        }
    }

    void set_community_regions_visible(bool visible) {
        for (auto& [cid, region] : community_regions_) {
            (void)cid;
            region->setVisible(visible);
        }
    }

    void add_labels(const Graph& g) {
        for (auto& [nid, item] : node_items_) {
            auto node_opt = g.getNode(nid);
            if (!node_opt) continue;

            auto* label = new QGraphicsSimpleTextItem(
                QString::fromStdString(node_opt->get().name), item);
            label->setPos(item->diameter() / 2 + 2, -6);
            label->setZValue(2.0);
            QFont f;
            f.setPointSize(8);
            label->setFont(f);
            label->setVisible(false);
        }
    }

    void store_graph(const Graph* g) { stored_graph_ = g; }

    void set_scene_rect(const QRectF& rect) { scene_->setSceneRect(rect); }

    void highlight_community(std::optional<uint32_t> cid) {
        for (auto& [nid, item] : node_items_) {
            (void)nid;
            if (cid && item->community_id() == *cid) {
                item->setZValue(2.0);
            } else {
                item->setZValue(1.0);
            }
        }

        for (auto& [eid, edge_item] : edge_items_) {
            (void)eid;
            if (!cid) {
                edge_item->setZValue(0.0);
                continue;
            }

            const Graph* g = stored_graph_;
            if (!g) {
                edge_item->setZValue(0.0);
                continue;
            }

            auto edge_it = g->getEdges().find(edge_item->edge_id());
            if (edge_it == g->getEdges().end()) {
                edge_item->setZValue(0.0);
                continue;
            }

            const auto& edge = edge_it->second;
            auto src_node_it = node_items_.find(edge.source);
            auto tgt_node_it = node_items_.find(edge.target);

            if (src_node_it != node_items_.end() &&
                tgt_node_it != node_items_.end() &&
                src_node_it->second->community_id() == *cid &&
                tgt_node_it->second->community_id() == *cid) {
                edge_item->setZValue(1.5);
            } else {
                edge_item->setZValue(0.0);
            }
        }
    }

    QRectF compute_scene_rect(const satgraf::layout::CoordinateMap& coords,
                              double margin = 50.0) const
    {
        if (coords.empty()) return QRectF(0, 0, 100, 100);

        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();

        for (const auto& [nid, c] : coords) {
            (void)nid;
            min_x = std::min(min_x, c.x);
            min_y = std::min(min_y, c.y);
            max_x = std::max(max_x, c.x);
            max_y = std::max(max_y, c.y);
        }

        return QRectF(min_x - margin, min_y - margin,
                      max_x - min_x + 2 * margin,
                      max_y - min_y + 2 * margin);
    }

    size_t node_count() const { return node_items_.size(); }
    size_t edge_count() const { return edge_items_.size(); }

    void set_community_click_callback(std::function<void(uint32_t)> cb) {
        community_click_callback_ = std::move(cb);
    }

    void render_simple_2d(
        const std::vector<satgraf::layout::Coordinate>& centers,
        const std::vector<std::size_t>& community_sizes,
        const std::vector<uint32_t>& community_ids,
        const std::map<std::pair<uint32_t, uint32_t>, double>& inter_community_edges,
        const std::unordered_map<uint32_t, QColor>& community_colors,
        double node_size,
        double avg_community_size,
        double edge_width = 1.0,
        bool show_labels = true)
    {
        scene_->clear();
        node_items_.clear();
        edge_items_.clear();
        community_regions_.clear();
        decision_highlight_ = nullptr;
        stored_graph_ = nullptr;
        simple2d_nodes_.clear();
        simple2d_edges_.clear();

        for (const auto& [pair, weight] : inter_community_edges) {
            QPointF from, to;
            bool found_from = false, found_to = false;
            for (size_t i = 0; i < community_ids.size(); ++i) {
                if (community_ids[i] == pair.first) {
                    from = QPointF(centers[i].x, centers[i].y);
                    found_from = true;
                }
                if (community_ids[i] == pair.second) {
                    to = QPointF(centers[i].x, centers[i].y);
                    found_to = true;
                }
            }
            if (!found_from || !found_to) continue;

            double pen_width = std::log2(weight) + edge_width;
            auto* line = new QGraphicsLineItem(from.x(), from.y(), to.x(), to.y());
            QPen pen(QBrush(QColor(180, 180, 180, 160)), pen_width);
            line->setPen(pen);
            line->setZValue(0.0);
            scene_->addItem(line);
            simple2d_edges_.push_back({pair.first, pair.second, line, pen});

            if (show_labels) {
                QPointF mid = (from + to) / 2.0;
                auto* weight_label = new QGraphicsTextItem(
                    QString::number(static_cast<int>(weight)));
                weight_label->setPos(mid);
                QFont f;
                f.setPointSize(8);
                weight_label->setFont(f);
                weight_label->setDefaultTextColor(QColor(200, 200, 200));
                weight_label->setZValue(0.5);
                scene_->addItem(weight_label);
            }
        }

        for (size_t i = 0; i < community_ids.size(); ++i) {
            uint32_t cid = community_ids[i];
            QPointF center(centers[i].x, centers[i].y);
            double diameter = (std::log2(static_cast<double>(community_sizes[i])) + node_size) * 2.0;

            auto color_it = community_colors.find(cid);
            QColor color = (color_it != community_colors.end())
                ? color_it->second : QColor(200, 200, 200);

            auto* ellipse = new CommunityEllipseItem(
                cid, center, diameter, color, community_click_callback_);
            scene_->addItem(ellipse);

            QGraphicsTextItem* label_item = nullptr;
            if (show_labels) {
                auto* label = new QGraphicsTextItem(
                    QString("C%1\n%2 nodes").arg(cid).arg(community_sizes[i]));
                label->setPos(center.x() + diameter / 2.0 + 4, center.y() - 12);
                label->setZValue(2.0);
                QFont f;
                f.setPointSize(9);
                f.setBold(true);
                label->setFont(f);
                label->setDefaultTextColor(QColor(220, 220, 220));
                scene_->addItem(label);
                label_item = label;
            }
            simple2d_nodes_[cid] = {ellipse, label_item};
        }
    }

    void highlight_simple2d_community(std::optional<uint32_t> cid) {
        if (simple2d_nodes_.empty()) return;

        if (!cid) {
            for (auto& [id, node] : simple2d_nodes_) {
                (void)id;
                node.ellipse->setZValue(1.0);
                node.ellipse->setOpacity(1.0);
                if (node.label) {
                    node.label->setZValue(2.0);
                    node.label->setOpacity(1.0);
                }
            }
            for (auto& e : simple2d_edges_) {
                e.line->setPen(e.original_pen);
                e.line->setZValue(0.0);
                e.line->setOpacity(1.0);
            }
            return;
        }

        std::set<uint32_t> neighbors;
        for (const auto& e : simple2d_edges_) {
            if (e.source == *cid) neighbors.insert(e.target);
            if (e.target == *cid) neighbors.insert(e.source);
        }

        for (auto& [id, node] : simple2d_nodes_) {
            if (id == *cid) {
                node.ellipse->setZValue(3.0);
                node.ellipse->setOpacity(1.0);
                if (node.label) {
                    node.label->setZValue(4.0);
                    node.label->setOpacity(1.0);
                }
            } else if (neighbors.count(id)) {
                node.ellipse->setZValue(2.5);
                node.ellipse->setOpacity(1.0);
                if (node.label) {
                    node.label->setZValue(3.5);
                    node.label->setOpacity(1.0);
                }
            } else {
                node.ellipse->setZValue(0.5);
                node.ellipse->setOpacity(0.3);
                if (node.label) {
                    node.label->setZValue(0.4);
                    node.label->setOpacity(0.3);
                }
            }
        }

        for (auto& e : simple2d_edges_) {
            bool connected = (e.source == *cid || e.target == *cid);
            if (connected) {
                e.line->setZValue(2.0);
                e.line->setOpacity(1.0);
                QPen p = e.original_pen;
                QColor c = p.color();
                c.setAlpha(255);
                p.setColor(c);
                p.setWidthF(p.widthF() * 1.5);
                e.line->setPen(p);
            } else {
                e.line->setPen(e.original_pen);
                e.line->setZValue(0.0);
                e.line->setOpacity(0.15);
            }
        }
    }

private:
    QGraphicsScene* scene_;
    std::unordered_map<satgraf::graph::NodeId, NodeGraphicsItem*> node_items_;
    std::unordered_map<satgraf::graph::EdgeId, EdgeGraphicsItem*> edge_items_;
    std::unordered_map<uint32_t, CommunityRegionItem*> community_regions_;
    DecisionHighlightItem* decision_highlight_{nullptr};
    const Graph* stored_graph_{nullptr};
    std::function<void(uint32_t)> community_click_callback_;

    struct Simple2DEdge {
        uint32_t source;
        uint32_t target;
        QGraphicsLineItem* line;
        QPen original_pen;
    };
    struct Simple2DNode {
        CommunityEllipseItem* ellipse;
        QGraphicsTextItem* label;
    };
    std::unordered_map<uint32_t, Simple2DNode> simple2d_nodes_;
    std::vector<Simple2DEdge> simple2d_edges_;
};

}  // namespace satgraf::rendering
