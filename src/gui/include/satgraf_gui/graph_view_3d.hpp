#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QColor>
#include <QRect>

#include <satgraf/layout.hpp>
#include <satgraf/graph.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace satgraf::gui {

// ---------------------------------------------------------------------------
// 10.1 — GraphView3D: OpenGL 3D widget for quotient graph rendering
// ---------------------------------------------------------------------------

class GraphView3D : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit GraphView3D(QWidget* parent = nullptr)
        : QOpenGLWidget(parent)
    {
        label_font_.setPointSize(10);
        label_font_.setBold(true);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setBackgroundColor(const QColor& color) {
        background_color_ = color;
        update();
    }

    void setGraph(
        const layout::Coordinate3DMap& positions,
        const std::vector<std::size_t>& community_sizes,
        const std::vector<uint32_t>& community_ids,
        const std::map<std::pair<uint32_t, uint32_t>, double>& inter_community_edges,
        const std::unordered_map<uint32_t, QColor>& community_colors,
        float node_size = 10.0f,
        double avg_community_size = 1.0,
        float edge_width = 1.0f)
    {
        communities_.clear();
        edges_.clear();
        full_graph_edges_.clear();
        full_graph_mode_ = false;
        edge_width_ = edge_width;

        if (positions.empty()) {
            update();
            return;
        }

        // Find bounding box of all positions
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();

        for (const auto& [nid, coord] : positions) {
            (void)nid;
            min_x = std::min(min_x, coord.x);
            max_x = std::max(max_x, coord.x);
            min_y = std::min(min_y, coord.y);
            max_y = std::max(max_y, coord.y);
            min_z = std::min(min_z, coord.z);
            max_z = std::max(max_z, coord.z);
        }

        const double span_x = std::max(max_x - min_x, 1e-9);
        const double span_y = std::max(max_y - min_y, 1e-9);
        const double span_z = std::max(max_z - min_z, 1e-9);
        const double max_span = std::max({span_x, span_y, span_z});
        const double center_x = (min_x + max_x) * 0.5;
        const double center_y = (min_y + max_y) * 0.5;
        const double center_z = (min_z + max_z) * 0.5;

        for (const auto& [nid, coord] : positions) {
            const auto idx = static_cast<std::size_t>(nid.value);
            const auto cid = (idx < community_ids.size()) ? community_ids[idx] : 0u;
            const auto count = (idx < community_sizes.size()) ? community_sizes[idx]
                                                              : std::size_t{1};

            CommunityNode3D node;
            node.position = QVector3D(
                static_cast<float>((coord.x - center_x) / max_span * 10.0),
                static_cast<float>((coord.y - center_y) / max_span * 10.0),
                static_cast<float>((coord.z - center_z) / max_span * 10.0));
            node.radius = 0.03f * node_size
                * std::cbrt(static_cast<float>(count) / static_cast<float>(avg_community_size));
            node.community_id = cid;
            node.node_count = count;

            auto color_it = community_colors.find(cid);
            node.color = (color_it != community_colors.end())
                             ? color_it->second
                             : QColor(200, 200, 200);

            communities_.push_back(std::move(node));
        }

        // Build inter-community edges
        for (const auto& [pair, weight] : inter_community_edges) {
            CommunityEdge3D edge;
            edge.source_id = pair.first;
            edge.target_id = pair.second;
            edge.weight = weight;
            edges_.push_back(std::move(edge));
        }

        camera_distance_ = 15.0f;
        update();
    }

    void setFullGraph(
        const satgraf::layout::Coordinate3DMap& positions,
        const std::unordered_map<satgraf::graph::NodeId, satgraf::graph::CommunityId>& community_assignment,
        const std::vector<QColor>& community_colors,
        float node_radius = 0.10f,
        const satgraf::graph::Graph<satgraf::graph::Node, satgraf::graph::Edge>* graph = nullptr,
        float edge_width = 1.0f)
    {
        communities_.clear();
        edges_.clear();
        full_graph_edges_.clear();
        full_graph_mode_ = true;
        edge_width_ = edge_width;

        if (positions.empty()) {
            update();
            return;
        }

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = std::numeric_limits<double>::max();
        double max_z = std::numeric_limits<double>::lowest();

        for (const auto& [nid, coord] : positions) {
            (void)nid;
            min_x = std::min(min_x, coord.x);
            max_x = std::max(max_x, coord.x);
            min_y = std::min(min_y, coord.y);
            max_y = std::max(max_y, coord.y);
            min_z = std::min(min_z, coord.z);
            max_z = std::max(max_z, coord.z);
        }

        const double span_x = std::max(max_x - min_x, 1e-9);
        const double span_y = std::max(max_y - min_y, 1e-9);
        const double span_z = std::max(max_z - min_z, 1e-9);
        const double max_span = std::max({span_x, span_y, span_z});
        const double center_x = (min_x + max_x) * 0.5;
        const double center_y = (min_y + max_y) * 0.5;
        const double center_z = (min_z + max_z) * 0.5;

        std::unordered_map<uint32_t, QVector3D> node_positions;
        std::unordered_map<uint32_t, uint32_t> node_community;
        std::unordered_map<uint32_t, QColor> node_color;

        for (const auto& [nid, coord] : positions) {
            uint32_t cid = 0;
            auto cit = community_assignment.find(nid);
            if (cit != community_assignment.end()) {
                cid = cit->second.value;
            }

            CommunityNode3D node;
            node.position = QVector3D(
                static_cast<float>((coord.x - center_x) / max_span * 10.0),
                static_cast<float>((coord.y - center_y) / max_span * 10.0),
                static_cast<float>((coord.z - center_z) / max_span * 10.0));
            node.radius = node_radius;
            node.community_id = cid;
            node.node_count = 1;

            QColor color = community_colors.empty()
                ? QColor(200, 200, 200)
                : community_colors[cid % community_colors.size()];
            node.color = color;

            node_positions[nid.value] = node.position;
            node_community[nid.value] = cid;
            node_color[nid.value] = color;
            node.node_id = nid.value;
            communities_.push_back(std::move(node));
        }

        if (graph) {
            for (const auto& [eid, edge] : graph->getEdges()) {
                (void)eid;
                auto src_it = node_positions.find(edge.source.value);
                auto tgt_it = node_positions.find(edge.target.value);
                if (src_it == node_positions.end() || tgt_it == node_positions.end()) continue;

                FullGraphEdge3D fge;
                fge.source_pos = src_it->second;
                fge.target_pos = tgt_it->second;
                fge.source_color = node_color[edge.source.value];
                fge.target_color = node_color[edge.target.value];
                fge.same_community = node_community[edge.source.value] == node_community[edge.target.value];
                fge.source_community = node_community[edge.source.value];
                fge.target_community = node_community[edge.target.value];
                fge.source_node = edge.source.value;
                fge.target_node = edge.target.value;
                full_graph_edges_.push_back(std::move(fge));
            }
        }

        camera_distance_ = 15.0f;
        update();
    }

    void highlightCommunity(std::optional<uint32_t> cid) {
        highlighted_community_ = cid;
        update();
    }

    QSize minimumSizeHint() const override { return QSize{400, 400}; }
    QSize sizeHint() const override { return QSize{800, 600}; }

signals:
    void communityClicked(uint32_t community_id);
    void nodeClicked(uint32_t node_id);

protected:
    // -------------------------------------------------------------------
    // 10.2 — OpenGL initialization
    // -------------------------------------------------------------------
    void initializeGL() override {
        initializeOpenGLFunctions();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_NORMALIZE);
        glShadeModel(GL_SMOOTH);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        // Directional light from upper-right-front
        const GLfloat light_pos[] = {5.0f, 5.0f, 10.0f, 1.0f};
        const GLfloat light_amb[] = {0.2f, 0.2f, 0.2f, 1.0f};
        const GLfloat light_dif[] = {0.8f, 0.8f, 0.8f, 1.0f};
        const GLfloat light_spc[] = {0.5f, 0.5f, 0.5f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_dif);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_spc);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        createSphereDisplayList(16, 12);
    }

    // -------------------------------------------------------------------
    // 10.2 — Perspective projection
    // -------------------------------------------------------------------
    void resizeGL(int w, int h) override {
        const float aspect = static_cast<float>(w) / std::max(h, 1);
        projection_matrix_.setToIdentity();
        projection_matrix_.perspective(45.0f, aspect, 0.1f, 100.0f);
        glViewport(0, 0, w, h);
    }

    // -------------------------------------------------------------------
    // 10.2 — Mouse interaction: arcball rotation + community picking
    // -------------------------------------------------------------------
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            last_mouse_pos_ = event->pos();

            if (communities_.empty()) return;

            const QPointF click_pos = event->position();
            float best_dist = std::numeric_limits<float>::max();
            uint32_t best_id = 0;
            bool found = false;

            for (const auto& comm : communities_) {
                const QVector3D screen = worldToScreen(comm.position);
                const QPointF center(screen.x(), screen.y());
                const float dist = static_cast<float>(
                    (click_pos - center).manhattanLength());

                const QVector3D top = worldToScreen(
                    comm.position + QVector3D(0.0f, comm.radius, 0.0f));
                const float screen_r = std::abs(top.y() - screen.y()) * 1.5f;

                if (dist < screen_r && dist < best_dist) {
                    best_dist = dist;
                    best_id = full_graph_mode_ ? comm.node_id : comm.community_id;
                    found = true;
                }
            }

            if (found) {
                if (full_graph_mode_) {
                    emit nodeClicked(best_id);
                } else {
                    emit communityClicked(best_id);
                }
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            const QPoint delta = event->pos() - last_mouse_pos_;
            rotation_y_ += static_cast<float>(delta.x()) * 0.5f;
            rotation_x_ += static_cast<float>(delta.y()) * 0.5f;
            rotation_x_ = std::clamp(rotation_x_, -89.0f, 89.0f);
            last_mouse_pos_ = event->pos();
            update();
        }
    }

    // -------------------------------------------------------------------
    // 10.2 — Scroll zoom
    // -------------------------------------------------------------------
    void wheelEvent(QWheelEvent* event) override {
        const float step = event->angleDelta().y() > 0 ? -1.0f : 1.0f;
        camera_distance_ += step * camera_distance_ * 0.1f;
        camera_distance_ = std::clamp(camera_distance_, 2.0f, 100.0f);
        event->accept();
        update();
    }

    // -------------------------------------------------------------------
    // 10.2-10.5 — Main paint routine
    // -------------------------------------------------------------------
    void paintGL() override {
        if (full_graph_mode_) {
            paintFullGraph();
        } else {
            paintSimple3D();
        }
    }

    void paintFullGraph() {
        glClearColor(background_color_.redF(), background_color_.greenF(),
                     background_color_.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_NORMALIZE);
        glShadeModel(GL_SMOOTH);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const qreal dpr = devicePixelRatio();
        glViewport(0, 0, static_cast<int>(width() * dpr), static_cast<int>(height() * dpr));

        const GLfloat lpos[] = {5.0f, 5.0f, 10.0f, 1.0f};
        const GLfloat lamb[] = {0.2f, 0.2f, 0.2f, 1.0f};
        const GLfloat ldif[] = {0.8f, 0.8f, 0.8f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, lpos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);

        float aspect = static_cast<float>(width()) / std::max(height(), 1);
        projection_matrix_.setToIdentity();
        projection_matrix_.perspective(45.0f, aspect, 0.1f, 100.0f);

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projection_matrix_.constData());

        modelview_matrix_.setToIdentity();
        modelview_matrix_.translate(0.0f, 0.0f, -camera_distance_);
        modelview_matrix_.rotate(rotation_x_, 1.0f, 0.0f, 0.0f);
        modelview_matrix_.rotate(rotation_y_, 0.0f, 1.0f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(modelview_matrix_.constData());

        // Prevent alpha writes — keeps framebuffer alpha at 1.0 so the
        // desktop compositor never sees through the window.
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        if (highlighted_community_) {
            const uint32_t hc = *highlighted_community_;

            std::set<uint32_t> linked_nodes;
            for (const auto& edge : full_graph_edges_) {
                if (edge.source_community == hc && edge.target_community != hc) {
                    linked_nodes.insert(edge.target_node);
                } else if (edge.target_community == hc && edge.source_community != hc) {
                    linked_nodes.insert(edge.source_node);
                }
            }

            glDisable(GL_LIGHTING);
            for (const auto& edge : full_graph_edges_) {
                const bool src_in = edge.source_community == hc;
                const bool tgt_in = edge.target_community == hc;

                if (src_in && tgt_in) {
                    glLineWidth(edge_width_ * 2.0f);
                    glColor4f(edge.source_color.redF(), edge.source_color.greenF(),
                              edge.source_color.blueF(), 0.8f);
                } else if (src_in || tgt_in) {
                    const QColor& hc_color = src_in ? edge.source_color : edge.target_color;
                    glLineWidth(edge_width_ * 1.5f);
                    glColor4f(hc_color.redF(), hc_color.greenF(),
                              hc_color.blueF(), 0.6f);
                } else {
                    glLineWidth(edge_width_);
                    glColor4f(0.3f, 0.3f, 0.3f, 0.05f);
                }
                glBegin(GL_LINES);
                glVertex3f(edge.source_pos.x(), edge.source_pos.y(), edge.source_pos.z());
                glVertex3f(edge.target_pos.x(), edge.target_pos.y(), edge.target_pos.z());
                glEnd();
            }
            glEnable(GL_LIGHTING);

            for (const auto& comm : communities_) {
                if (comm.community_id == hc) {
                    drawSphere(comm.position, comm.radius * 1.3f, comm.color, 1.0f);
                } else if (linked_nodes.count(comm.node_id)) {
                    drawSphere(comm.position, comm.radius, comm.color, 0.7f);
                } else {
                    drawSphere(comm.position, comm.radius, comm.color, 0.1f);
                }
            }
        } else {
            glDisable(GL_LIGHTING);
            glLineWidth(edge_width_);
            for (const auto& edge : full_graph_edges_) {
                if (edge.same_community) {
                    glColor4f(edge.source_color.redF(), edge.source_color.greenF(),
                              edge.source_color.blueF(), 0.4f);
                } else {
                    glColor4f(0.7f, 0.7f, 0.7f, 0.15f);
                }
                glBegin(GL_LINES);
                glVertex3f(edge.source_pos.x(), edge.source_pos.y(), edge.source_pos.z());
                glVertex3f(edge.target_pos.x(), edge.target_pos.y(), edge.target_pos.z());
                glEnd();
            }
            glEnable(GL_LIGHTING);

            for (const auto& comm : communities_) {
                drawSphere(comm.position, comm.radius, comm.color);
            }
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    void paintSimple3D() {
        struct LabelData {
            QPointF screen_pos;
            QString text;
            QColor color;
        };
        std::vector<LabelData> labels;
        labels.reserve(communities_.size());

        QPainter painter(this);
        painter.beginNativePainting();

        glClearColor(background_color_.redF(), background_color_.greenF(),
                     background_color_.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_NORMALIZE);
        glShadeModel(GL_SMOOTH);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const qreal dpr = devicePixelRatio();
        glViewport(0, 0, static_cast<int>(width() * dpr), static_cast<int>(height() * dpr));

        // Re-upload light state
        const GLfloat lpos[] = {5.0f, 5.0f, 10.0f, 1.0f};
        const GLfloat lamb[] = {0.2f, 0.2f, 0.2f, 1.0f};
        const GLfloat ldif[] = {0.8f, 0.8f, 0.8f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, lpos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lamb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, ldif);

        // Projection matrix
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(projection_matrix_.constData());

        // Modelview matrix: pull camera back, then rotate
        modelview_matrix_.setToIdentity();
        modelview_matrix_.translate(0.0f, 0.0f, -camera_distance_);
        modelview_matrix_.rotate(rotation_x_, 1.0f, 0.0f, 0.0f);
        modelview_matrix_.rotate(rotation_y_, 0.0f, 1.0f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(modelview_matrix_.constData());

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        // ---------------------------------------------------------------
        // 10.4 — Draw inter-community edges as thick lines
        // ---------------------------------------------------------------
        glDisable(GL_LIGHTING);
        for (const auto& edge : edges_) {
            QVector3D src_pos;
            QVector3D tgt_pos;
            bool found_src = false;
            bool found_tgt = false;

            for (const auto& comm : communities_) {
                if (comm.community_id == edge.source_id) {
                    src_pos = comm.position;
                    found_src = true;
                }
                if (comm.community_id == edge.target_id) {
                    tgt_pos = comm.position;
                    found_tgt = true;
                }
            }

            if (!found_src || !found_tgt) continue;

            const float line_w = edge_width_ * (1.0f + std::log(
                static_cast<float>(edge.weight) + 1.0f) * 0.5f);
            glLineWidth(std::max(line_w, 1.0f));
            glColor4f(0.8f, 0.8f, 0.8f, 0.6f);

            glBegin(GL_LINES);
            glVertex3f(src_pos.x(), src_pos.y(), src_pos.z());
            glVertex3f(tgt_pos.x(), tgt_pos.y(), tgt_pos.z());
            glEnd();
        }
        glEnable(GL_LIGHTING);

        // ---------------------------------------------------------------
        // 10.3 — Draw community spheres
        // ---------------------------------------------------------------
        for (const auto& comm : communities_) {
            drawSphere(comm.position, comm.radius, comm.color);
        }

        for (const auto& comm : communities_) {
            const QVector3D label_world = comm.position +
                QVector3D(0.0f, comm.radius + 0.3f, 0.0f);
            const QVector3D screen = worldToScreen(label_world);

            if (screen.z() >= 0.0f && screen.z() <= 1.0f) {
                LabelData lbl;
                lbl.screen_pos = QPointF(screen.x(), screen.y());
                lbl.text = QString("C%1\n%2 nodes")
                    .arg(comm.community_id)
                    .arg(comm.node_count);
                lbl.color = QColor(255, 255, 255);
                labels.push_back(std::move(lbl));
            }
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        painter.endNativePainting();

        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setFont(label_font_);

        for (const auto& lbl : labels) {
            drawTextBillboard(painter, lbl.screen_pos, lbl.text, lbl.color);
        }
    }

private:
    // Camera state
    float camera_distance_{15.0f};
    float rotation_x_{25.0f};  // pitch (degrees)
    float rotation_y_{45.0f};  // yaw   (degrees)
    QPoint last_mouse_pos_;

    // Graph data structures
    struct CommunityNode3D {
        QVector3D position;
        float radius{0.3f};
        uint32_t community_id{0};
        uint32_t node_id{0};
        QColor color;
        std::size_t node_count{0};
    };

    struct CommunityEdge3D {
        uint32_t source_id{0};
        uint32_t target_id{0};
        double weight{0.0};
    };

    struct FullGraphEdge3D {
        QVector3D source_pos;
        QVector3D target_pos;
        QColor source_color;
        QColor target_color;
        bool same_community{false};
        uint32_t source_community{0};
        uint32_t target_community{0};
        uint32_t source_node{0};
        uint32_t target_node{0};
    };

    std::vector<CommunityNode3D> communities_;
    std::vector<CommunityEdge3D> edges_;
    std::vector<FullGraphEdge3D> full_graph_edges_;
    bool full_graph_mode_{false};
    float edge_width_{1.0f};
    std::optional<uint32_t> highlighted_community_{};
    QColor background_color_{0, 0, 0};

    QMatrix4x4 projection_matrix_;
    QMatrix4x4 modelview_matrix_;

    QFont label_font_;

    // Sphere display list handle
    GLuint sphere_list_{0};

    // -------------------------------------------------------------------
    // 10.3 — Sphere display list creation (parametric tessellation)
    // -------------------------------------------------------------------
    void createSphereDisplayList(int slices, int stacks) {
        sphere_list_ = glGenLists(1);
        if (sphere_list_ == 0) return;

        glNewList(sphere_list_, GL_COMPILE);
        for (int i = 0; i < stacks; ++i) {
            const float lat0 = static_cast<float>(M_PI) *
                (-0.5f + static_cast<float>(i) / stacks);
            const float z0 = std::sin(lat0);
            const float r0 = std::cos(lat0);

            const float lat1 = static_cast<float>(M_PI) *
                (-0.5f + static_cast<float>(i + 1) / stacks);
            const float z1 = std::sin(lat1);
            const float r1 = std::cos(lat1);

            glBegin(GL_TRIANGLE_STRIP);
            for (int j = 0; j <= slices; ++j) {
                const float lng = 2.0f * static_cast<float>(M_PI) *
                    static_cast<float>(j) / slices;
                const float cos_lng = std::cos(lng);
                const float sin_lng = std::sin(lng);

                glNormal3f(cos_lng * r0, sin_lng * r0, z0);
                glVertex3f(cos_lng * r0, sin_lng * r0, z0);
                glNormal3f(cos_lng * r1, sin_lng * r1, z1);
                glVertex3f(cos_lng * r1, sin_lng * r1, z1);
            }
            glEnd();
        }
        glEndList();
    }

    // -------------------------------------------------------------------
    // 10.3 — Draw a single community sphere
    // -------------------------------------------------------------------
    void drawSphere(const QVector3D& center, float radius,
                    const QColor& color, float alpha = -1.0f,
                    int slices = 16, int stacks = 12)
    {
        glPushMatrix();
        glTranslatef(center.x(), center.y(), center.z());
        glScalef(radius, radius, radius);

        const float a = (alpha >= 0.0f) ? alpha : color.alphaF();
        glColor4f(color.redF(), color.greenF(), color.blueF(), a);

        if (sphere_list_ != 0) {
            glCallList(sphere_list_);
        } else {
            // Fallback: draw sphere geometry directly
            draw_sphere_geometry(slices, stacks);
        }

        glPopMatrix();
    }

    void draw_sphere_geometry(int slices, int stacks) const {
        for (int i = 0; i < stacks; ++i) {
            const float lat0 = static_cast<float>(M_PI) *
                (-0.5f + static_cast<float>(i) / stacks);
            const float z0 = std::sin(lat0);
            const float r0 = std::cos(lat0);

            const float lat1 = static_cast<float>(M_PI) *
                (-0.5f + static_cast<float>(i + 1) / stacks);
            const float z1 = std::sin(lat1);
            const float r1 = std::cos(lat1);

            glBegin(GL_TRIANGLE_STRIP);
            for (int j = 0; j <= slices; ++j) {
                const float lng = 2.0f * static_cast<float>(M_PI) *
                    static_cast<float>(j) / slices;
                const float cos_lng = std::cos(lng);
                const float sin_lng = std::sin(lng);

                glNormal3f(cos_lng * r0, sin_lng * r0, z0);
                glVertex3f(cos_lng * r0, sin_lng * r0, z0);
                glNormal3f(cos_lng * r1, sin_lng * r1, z1);
                glVertex3f(cos_lng * r1, sin_lng * r1, z1);
            }
            glEnd();
        }
    }

    // -------------------------------------------------------------------
    // 10.4 — Cylinder placeholder (edges use GL_LINES; kept for future)
    // -------------------------------------------------------------------
    void drawCylinder(const QVector3D& start, const QVector3D& end,
                      float radius, const QColor& color, int segments = 8)
    {
        (void)start;
        (void)end;
        (void)radius;
        (void)color;
        (void)segments;
    }

    // -------------------------------------------------------------------
    // 10.5 — Billboard text label drawing (QPainter 2D overlay)
    // -------------------------------------------------------------------
    void drawTextBillboard(QPainter& painter, const QPointF& screen_pos,
                           const QString& text, const QColor& color)
    {
        const QFontMetrics fm(painter.font());
        QRectF bounds = fm.boundingRect(
            QRect(0, 0, 200, 100),
            Qt::AlignCenter | Qt::TextDontClip, text);
        bounds.moveCenter(screen_pos.toPoint());

        // Semi-transparent background for readability
        painter.fillRect(bounds.adjusted(-4, -2, 4, 2), QColor(0, 0, 0, 160));
        painter.setPen(color);
        painter.drawText(bounds, Qt::AlignCenter | Qt::TextDontClip, text);
    }

    // -------------------------------------------------------------------
    // 3D world position → 2D screen coordinates
    // -------------------------------------------------------------------
    QVector3D worldToScreen(const QVector3D& world_pos) const {
        const QMatrix4x4 mvp = projection_matrix_ * modelview_matrix_;
        const QVector4D clip = mvp * QVector4D(world_pos, 1.0f);

        if (qFuzzyIsNull(clip.w())) {
            return QVector3D{};
        }

        const float inv_w = 1.0f / clip.w();
        const float ndc_x = clip.x() * inv_w;
        const float ndc_y = clip.y() * inv_w;
        const float ndc_z = clip.z() * inv_w;

        // NDC [-1,1] → screen pixels, Y flipped for Qt coordinates
        const float sx = (ndc_x + 1.0f) * 0.5f * static_cast<float>(width());
        const float sy = (1.0f - ndc_y) * 0.5f * static_cast<float>(height());

        return QVector3D{sx, sy, ndc_z};
    }
};

}  // namespace satgraf::gui
