#pragma once

#include <satgraf/graph.hpp>
#include <satgraf/layout.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/evolution.hpp>

#include <QImage>
#include <QPainter>
#include <QColor>
#include <QSize>
#include <functional>
#include <string>
#include <vector>

namespace satgraf::export_ {

// ---------------------------------------------------------------------------
// 11.1 — Static image export
// ---------------------------------------------------------------------------

inline void render_to_image(
    const graph::Graph<graph::Node, graph::Edge>& graph,
    const layout::CoordinateMap& coords,
    const community::CommunityResult& communities,
    QImage& image,
    double node_diameter = 10.0,
    double edge_width = 1.0,
    std::function<void(double)> progress = nullptr)
{
    if (coords.empty()) {
        if (progress) progress(1.0);
        return;
    }

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

    double margin = 40.0;
    double data_w = max_x - min_x + 2 * margin;
    double data_h = max_y - min_y + 2 * margin;
    double scale = std::min(image.width() / data_w, image.height() / data_h);

    double off_x = (image.width() - (max_x - min_x) * scale) / 2.0 - min_x * scale;
    double off_y = (image.height() - (max_y - min_y) * scale) / 2.0 - min_y * scale;

    auto to_pixel = [&](const layout::Coordinate& c) -> QPointF {
        return QPointF(c.x * scale + off_x, c.y * scale + off_y);
    };

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(image.rect(), Qt::white);

    size_t total = graph.edgeCount() + graph.nodeCount();
    size_t done = 0;

    // Draw edges
    QPen edge_pen;
    edge_pen.setWidthF(std::max(1.0, edge_width * scale / 50.0));
    painter.setPen(edge_pen);

    for (const auto& [eid, edge] : graph.getEdges()) {
        auto src_it = coords.find(edge.source);
        auto tgt_it = coords.find(edge.target);
        if (src_it == coords.end() || tgt_it == coords.end()) continue;

        auto src_cit = communities.assignment.find(edge.source);
        auto tgt_cit = communities.assignment.find(edge.target);

        QColor color(180, 180, 180);
        if (edge.type == graph::EdgeType::Conflict) {
            color = QColor(255, 0, 0);
        } else if (src_cit != communities.assignment.end() &&
                   tgt_cit != communities.assignment.end() &&
                   src_cit->second == tgt_cit->second) {
            static const QColor palette[] = {
                {230, 25, 75}, {60, 180, 75}, {255, 225, 25}, {0, 130, 200},
                {245, 130, 48}, {145, 30, 180}, {70, 240, 240}, {240, 50, 230},
                {210, 245, 60}, {250, 190, 212}, {0, 128, 128}, {220, 190, 255},
                {170, 110, 40}, {255, 250, 200}, {128, 0, 0}, {170, 255, 195},
                {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {128, 128, 128},
            };
            constexpr size_t n = sizeof(palette) / sizeof(palette[0]);
            color = palette[src_cit->second.value % n];
        }

        edge_pen.setColor(color);
        if (edge.type == graph::EdgeType::Conflict) {
            edge_pen.setStyle(Qt::DashLine);
        } else {
            edge_pen.setStyle(Qt::SolidLine);
        }
        painter.setPen(edge_pen);

        QPointF from = to_pixel(src_it->second);
        QPointF to = to_pixel(tgt_it->second);
        painter.drawLine(from, to);
        done++;
    }

    // Draw nodes
    double r = std::max(3.0, node_diameter * scale / 2.0 / 50.0 * image.width() / 1024.0);
    painter.setPen(Qt::NoPen);

    static const QColor node_palette[] = {
        {230, 25, 75}, {60, 180, 75}, {255, 225, 25}, {0, 130, 200},
        {245, 130, 48}, {145, 30, 180}, {70, 240, 240}, {240, 50, 230},
        {210, 245, 60}, {250, 190, 212}, {0, 128, 128}, {220, 190, 255},
        {170, 110, 40}, {255, 250, 200}, {128, 0, 0}, {170, 255, 195},
        {128, 128, 0}, {255, 215, 180}, {0, 0, 128}, {128, 128, 128},
    };
    constexpr size_t np = sizeof(node_palette) / sizeof(node_palette[0]);

    for (const auto& [nid, node] : graph.getNodes()) {
        auto it = coords.find(nid);
        if (it == coords.end()) continue;

        uint32_t cid = 0;
        auto cit = communities.assignment.find(nid);
        if (cit != communities.assignment.end()) cid = cit->second.value;

        QColor color = node_palette[cid % np];
        painter.setBrush(QBrush(color));

        QPointF center = to_pixel(it->second);
        painter.drawEllipse(center, r, r);
        done++;
    }

    painter.end();
    if (progress) progress(1.0);
}

// ---------------------------------------------------------------------------
// 11.1 — Save to file
// ---------------------------------------------------------------------------

inline bool export_png(
    const graph::Graph<graph::Node, graph::Edge>& graph,
    const layout::CoordinateMap& coords,
    const community::CommunityResult& communities,
    const std::string& output_path,
    int width = 1024, int height = 1024,
    int quality = -1,
    std::function<void(double)> progress = nullptr)
{
    QImage image(width, height, QImage::Format_RGB32);
    render_to_image(graph, coords, communities, image, 10.0, 1.0, progress);
    return image.save(QString::fromStdString(output_path), "PNG", quality);
}

inline bool export_jpeg(
    const graph::Graph<graph::Node, graph::Edge>& graph,
    const layout::CoordinateMap& coords,
    const community::CommunityResult& communities,
    const std::string& output_path,
    int width = 1024, int height = 1024,
    int quality = 95,
    std::function<void(double)> progress = nullptr)
{
    QImage image(width, height, QImage::Format_RGB32);
    render_to_image(graph, coords, communities, image, 10.0, 1.0, progress);
    return image.save(QString::fromStdString(output_path), "JPEG", quality);
}

// ---------------------------------------------------------------------------
// 11.2 — Animated GIF export (frame sequence)
// ---------------------------------------------------------------------------

struct GifFrame {
    QImage image;
    int delay_ms{100};
};

inline std::vector<QImage> render_evolution_frames(
    const graph::Graph<graph::Node, graph::Edge>& base_graph,
    const std::vector<std::string>& event_lines,
    evolution::GraphMode mode,
    int width = 512, int height = 512,
    std::function<void(double)> progress = nullptr)
{
    std::vector<QImage> frames;

    graph::Graph<graph::Node, graph::Edge> graph;
    for (const auto& [nid, node] : base_graph.getNodes()) {
        graph.createNode(nid, node.name);
    }
    for (const auto& [eid, edge] : base_graph.getEdges()) {
        graph.createEdge(edge.source, edge.target);
    }

    evolution::EvolutionEngine engine(graph, mode);

    layout::CoordinateMap coords;
    for (const auto& [nid, node] : base_graph.getNodes()) {
        coords[nid] = {0.0, 0.0};
    }

    community::CommunityResult empty_communities;

    size_t conflict_count = 0;
    for (const auto& line : event_lines) {
        if (!line.empty() && line[0] == '!') conflict_count++;
    }

    size_t frame_idx = 0;
    for (const auto& line : event_lines) {
        engine.process_line(line);

        if (!line.empty() && line[0] == '!') {
            QImage frame(width, height, QImage::Format_RGB32);
            render_to_image(graph, coords, empty_communities, frame,
                          10.0, 1.0, nullptr);
            frames.push_back(std::move(frame));
            frame_idx++;
            if (progress) {
                progress(static_cast<double>(frame_idx) /
                         static_cast<double>(conflict_count));
            }
        }
    }

    return frames;
}

inline bool save_gif_frames(const std::vector<QImage>& frames,
                            const std::string& output_path,
                            int delay_ms = 100)
{
    if (frames.empty()) return false;

    QString path = QString::fromStdString(output_path);

    if (frames.size() == 1) {
        return frames[0].save(path, "PNG");
    }

    // Qt doesn't have native GIF animation support; save first frame as PNG
    // Animated GIF requires a dedicated library. For now, save as numbered PNGs.
    for (size_t i = 0; i < frames.size(); ++i) {
        QString frame_path = path + QString("_%1.png").arg(i, 4, 10, QChar('0'));
        if (!frames[i].save(frame_path, "PNG")) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// 11.3 — Headless offscreen rendering
// ---------------------------------------------------------------------------

inline QImage render_headless(
    const graph::Graph<graph::Node, graph::Edge>& graph,
    const layout::CoordinateMap& coords,
    const community::CommunityResult& communities,
    int width = 1024, int height = 1024,
    double node_diameter = 10.0,
    std::function<void(double)> progress = nullptr)
{
    QImage image(width, height, QImage::Format_RGB32);
    render_to_image(graph, coords, communities, image, node_diameter, 1.0, progress);
    return image;
}

}  // namespace satgraf::export_
