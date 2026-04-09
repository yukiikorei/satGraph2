#pragma once

#include <satgraf/dimacs_parser.hpp>
#include <satgraf/graph.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/layout.hpp>
#include <satgraf/evolution.hpp>
#include <satgraf/solver.hpp>
#include <satgraf_gui/graph_renderer.hpp>
#include <satgraf_gui/graph_view_3d.hpp>
#include <satgraf_gui/export.hpp>

#include <QMainWindow>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QStatusBar>
#include <QApplication>
#include <QSettings>
#include <QLineEdit>
#include <QFrame>
#include <QTimer>
#include <QWindow>
#include <QPainter>
#include <QPainterPath>
#include <numeric>
#include <string>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <unordered_map>

namespace satgraf::gui {

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* window, QWidget* parent = nullptr)
        : QWidget(parent), window_(window)
    {
        setFixedHeight(32);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);
        layout->setSpacing(6);

        auto* close_btn = new QPushButton("✕", this);
        auto* min_btn = new QPushButton("−", this);
        auto* max_btn = new QPushButton("⤢", this);

        for (auto* btn : {close_btn, min_btn, max_btn}) {
            btn->setFixedSize(28, 28);
            btn->setFlat(true);
            btn->setFocusPolicy(Qt::NoFocus);
        }

        connect(close_btn, &QPushButton::clicked, window_, &QWidget::close);
        connect(min_btn, &QPushButton::clicked, window_, &QWidget::showMinimized);
        connect(max_btn, &QPushButton::clicked, this, [this] {
            if (window_->isMaximized()) window_->showNormal();
            else window_->showMaximized();
        });

        layout->addWidget(close_btn);
        layout->addWidget(min_btn);
        layout->addWidget(max_btn);
        layout->addSpacing(8);

        auto* title = new QLabel("satGraf", this);
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title, 1);

        layout->addSpacing(28 * 3 + 6 * 2 + 8);
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override {
        if (window_->isMaximized()) window_->showNormal();
        else window_->showMaximized();
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            if (window_->windowHandle()) {
                window_->windowHandle()->startSystemMove();
            }
            dragging_ = true;
            drag_start_ = e->globalPosition().toPoint() - window_->pos();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_ && (e->buttons() & Qt::LeftButton)) {
            if (window_->isMaximized()) {
                window_->showNormal();
                drag_start_ = QPoint(window_->width() / 2, 16);
            }
            window_->move(e->globalPosition().toPoint() - drag_start_);
        }
    }

    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = false; }

private:
    QWidget* window_;
    bool dragging_ = false;
    QPoint drag_start_;
};

enum class RenderMode { Detailed2D, Simple2D, Mode3D, Simple3D };

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("satGraf");
        resize(1440, 900);
        setWindowFlag(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);

        setup_menus();
        setup_central_widget();

        statusBar()->hide();
        menuBar()->hide();
    }

    void resizeEvent(QResizeEvent*) override {
        QPainterPath path;
        path.addRoundedRect(rect(), 10, 10);
        QRegion mask = QRegion(path.toFillPolygon().toPolygon());
        setMask(mask);
    }

signals:
    void file_opened(const QString& path);

public slots:
    void open_file() {
        QString path = QFileDialog::getOpenFileName(
            this, "Open DIMACS CNF File", {},
            "CNF Files (*.cnf);;All Files (*)");
        if (path.isEmpty()) return;
        open_file_path(path);
    }

    void open_file_path(const QString& path) {
        try {
            dimacs::Parser parser;
            auto g = parser.parse(path.toStdString(), dimacs::Mode::VIG);
            graph_ = std::make_unique<Graph>(std::move(g));
            cnf_path_ = path;

            file_label_->setText(QFileInfo(path).fileName());
            file_label_->setToolTip(path);
            stats_nodes_->setText(QString::number(graph_->nodeCount()));
            stats_edges_->setText(QString::number(graph_->edgeCount()));
            stats_communities_->setText("—");
            stats_modularity_->setText("—");
            stats_internal_edges_->setText("—");
            stats_external_edges_->setText("—");

            render_btn_->setEnabled(true);
            start_solver_btn_->setEnabled(true);
            log("Loaded " + path);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", e.what());
        }
    }

    void render_graph() {
        if (!graph_) {
            log("Open a CNF file first");
            return;
        }

        if (rendering_active_.load()) {
            render_cancel_.store(true);
            rendering_active_.store(false);
            render_btn_->setText("▶  Render");
            set_controls_enabled(true);
            log("Cancelled");
            return;
        }

        if (render_thread_ && render_thread_->joinable()) {
            render_cancel_.store(true);
            render_thread_->join();
            render_thread_.reset();
        }

        if (evolution_active_) {
            solver_.cancel();
            if (solver_timer_) solver_timer_->stop();
            evolution_active_ = false;
            render_mode_combo_->setEnabled(true);
            start_solver_btn_->setText("▶  Start Solver");
            start_solver_btn_->disconnect();
            connect(start_solver_btn_, &QPushButton::clicked, this, &MainWindow::start_solver);
            log("Solver stopped for re-render");
        }

        mode_cache_.clear();

        set_controls_enabled(false);
        rendering_active_.store(true);
        render_cancel_.store(false);
        render_btn_->setText("⏹  Cancel");
        render_btn_->setEnabled(true);
        log("Detecting communities...");

        const uint64_t gen = ++render_generation_;

        render_thread_.emplace(std::thread([this, gen] {
            try {
                auto detector = community::DetectorFactory::instance().create(community_method_);
                auto communities = detector->detect(*graph_);

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                QMetaObject::invokeMethod(this, [this] { log("Computing bridge nodes..."); }, Qt::QueuedConnection);
                auto bridge_nodes = satgraf::community::detail::compute_bridge_nodes(communities, *graph_);

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                QMetaObject::invokeMethod(this, [this] { log("Computing quotient graph..."); }, Qt::QueuedConnection);
                auto quotient_graph = satgraf::layout::CommunityLayoutSupport::build_quotient_graph(*graph_, communities.assignment);

                std::vector<std::size_t> community_sizes;
                std::vector<uint32_t> community_ids;
                for (const auto& community : quotient_graph.communities) {
                    community_sizes.push_back(community.size());
                    auto cid = communities.assignment.at(community.front());
                    community_ids.push_back(cid.value);
                }
                std::map<std::pair<uint32_t, uint32_t>, double> inter_community_edges;
                for (const auto& [eid, edge] : graph_->getEdges()) {
                    (void)eid;
                    auto src_it = communities.assignment.find(edge.source);
                    auto tgt_it = communities.assignment.find(edge.target);
                    if (src_it == communities.assignment.end() || tgt_it == communities.assignment.end()) continue;
                    auto src_cid = src_it->second.value;
                    auto tgt_cid = tgt_it->second.value;
                    if (src_cid == tgt_cid) continue;
                    auto ordered = std::minmax(src_cid, tgt_cid);
                    inter_community_edges[ordered] += edge.weight;
                }

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                int iters = iterations_slider_ ? iterations_slider_->value() : 20;
                QMetaObject::invokeMethod(this, [this] { log("Computing layout..."); }, Qt::QueuedConnection);
                auto mode_entry = compute_mode_layout(render_mode_, *graph_, communities, quotient_graph, iters);

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                QMetaObject::invokeMethod(this, [this, gen,
                    c = std::move(communities),
                    bn = std::move(bridge_nodes),
                    qg = std::move(quotient_graph),
                    cs = std::move(community_sizes),
                    ci = std::move(community_ids),
                    ice = std::move(inter_community_edges),
                    me = std::move(mode_entry)
                ]() mutable {
                    on_render_complete(gen, std::move(c), std::move(bn), std::move(qg),
                                       std::move(cs), std::move(ci), std::move(ice), std::move(me));
                }, Qt::QueuedConnection);
            } catch (const std::exception& e) {
                QMetaObject::invokeMethod(this, [this, err = std::string(e.what())] {
                    on_render_error(QString::fromStdString(err));
                }, Qt::QueuedConnection);
            }
        }));
    }

    void on_render_cancelled() {
        if (!rendering_active_.load()) return;
        rendering_active_.store(false);
        render_btn_->setText("▶  Render");
        set_controls_enabled(true);
        log("Render cancelled");
    }

    void on_render_error(QString error) {
        rendering_active_.store(false);
        render_btn_->setText("▶  Render");
        set_controls_enabled(true);
        log("Error: " + error);
    }

    void set_controls_enabled(bool enabled) {
        community_combo_->setEnabled(enabled);
        layout_combo_->setEnabled(enabled);
        render_mode_combo_->setEnabled(enabled && !evolution_active_);
        iterations_slider_->setEnabled(enabled);
        node_size_slider_->setEnabled(enabled);
        edge_size_slider_->setEnabled(enabled);
        community_region_check_->setEnabled(enabled);
        community_select_combo_->setEnabled(enabled);
        start_solver_btn_->setEnabled(enabled && graph_ != nullptr);
        step_back_btn_->setEnabled(enabled && engine_ && engine_->history_depth() > 0);
        timeline_slider_->setEnabled(enabled);
    }

    void export_image() {
        QString path = QFileDialog::getSaveFileName(
            this, "Export Image", {},
            "PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)");
        if (path.isEmpty()) return;

        if (!coords_.empty()) {
            std::string sp = path.toStdString();
            if (path.endsWith(".jpg", Qt::CaseInsensitive)) {
                satgraf::export_::export_jpeg(*graph_, coords_, communities_, sp,
                          1024, 1024, 95);
            } else {
                satgraf::export_::export_png(*graph_, coords_, communities_, sp,
                         1024, 1024);
            }
            log("Exported: " + path);
        }
    }

    void zoom_in() { if (view_) view_->zoom_in(); }
    void zoom_out() { if (view_) view_->zoom_out(); }
    void fit_to_view() { if (view_) view_->fit_all(); }

    void show_about() {
        QMessageBox::about(this, "About satGraf",
            "satGraf v0.1.0\n\nSAT solver visualization tool\nC++17 + Qt 6 + OpenCL");
    }

    void set_layout_algorithm(const QString& name) {
        layout_name_ = name.toStdString();
        mode_layout_selection_[static_cast<int>(render_mode_)] = layout_name_;
    }

    void set_community_method(const QString& name) {
        community_method_ = name.toStdString();
    }

    void on_render_mode_changed(int index) {
        switch (index) {
            case 0: render_mode_ = RenderMode::Detailed2D; break;
            case 1: render_mode_ = RenderMode::Simple2D; break;
            case 2: render_mode_ = RenderMode::Mode3D; break;
            case 3: render_mode_ = RenderMode::Simple3D; break;
            default: render_mode_ = RenderMode::Detailed2D; break;
        }

        update_layout_combo_for_mode();

        auto cache_it = mode_cache_.find(render_mode_);
        if (cache_it != mode_cache_.end()) {
            perform_render();
            return;
        }

        if (communities_.assignment.empty() || !graph_) return;

        if (render_thread_ && render_thread_->joinable()) {
            render_cancel_.store(true);
            render_thread_->join();
            render_thread_.reset();
        }

        set_controls_enabled(false);
        rendering_active_.store(true);
        render_cancel_.store(false);
        render_btn_->setText("⏹  Cancel");
        render_btn_->setEnabled(true);
        log("Computing layout for mode...");

        const uint64_t gen = ++render_generation_;

        render_thread_.emplace(std::thread([this, gen] {
            try {
                int iters = iterations_slider_ ? iterations_slider_->value() : 20;
                auto entry = compute_mode_layout(render_mode_, *graph_, communities_, quotient_graph_, iters);

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                QMetaObject::invokeMethod(this, [this, gen, e = std::move(entry)]() mutable {
                    if (gen != render_generation_.load()) return;
                    if (render_cancel_.load()) return;

                    mode_cache_[render_mode_] = std::move(e);
                    if (render_mode_ == RenderMode::Detailed2D) {
                        coords_ = mode_cache_[RenderMode::Detailed2D].coords;
                    }

                    perform_render();

                    rendering_active_.store(false);
                    render_btn_->setText("▶  Render");
                    set_controls_enabled(true);
                    log("Mode layout complete");
                }, Qt::QueuedConnection);
            } catch (const std::exception& ex) {
                QMetaObject::invokeMethod(this, [this, err = std::string(ex.what())] {
                    on_render_error(QString::fromStdString(err));
                }, Qt::QueuedConnection);
            }
        }));
    }

    void set_background(const QString& color_name) {
        if (!view_) return;
        if (color_name == "Dark") {
            bg_brush_ = QBrush(Qt::black);
        } else if (color_name == "Light Gray") {
            bg_brush_ = QBrush(QColor(240, 240, 240));
        } else if (color_name == "White") {
            bg_brush_ = QBrush(Qt::white);
        } else {
            bg_brush_ = QBrush(Qt::transparent);
        }
        view_->setBackgroundBrush(bg_brush_);
        view_->scene()->setBackgroundBrush(bg_brush_);
        view_->viewport()->update();
        if (view_3d_) {
            QColor bg3d = bg_brush_.color();
            if (!bg3d.isValid() || bg3d.alpha() == 0) bg3d = QColor(0, 0, 0);
            view_3d_->setBackgroundColor(bg3d);
        }
    }

    void on_community_selected(int index) {
        if (index < 0 || !graph_) return;
        uint32_t cid = static_cast<uint32_t>(community_select_combo_->itemData(index).toUInt());

        if (cid == 0xFFFFFFFF) {
            comm_nodes_label_->setText("—");
            comm_bridge_label_->setText("—");
            comm_internal_edges_label_->setText("—");
            comm_external_edges_label_->setText("—");
            comm_linked_label_->setText("—");
            selected_community_ = std::nullopt;
            renderer_->highlight_community(std::nullopt);
            renderer_->highlight_simple2d_community(std::nullopt);
            if (view_3d_) view_3d_->highlightCommunity(std::nullopt);
            update_neighbor_community_list(0);
            return;
        }

        selected_community_ = cid;
        if (highlight_check_->isChecked()) {
            renderer_->highlight_community(cid);
            renderer_->highlight_simple2d_community(cid);
            if (render_mode_ == RenderMode::Mode3D && view_3d_ && view_3d_->isVisible()) {
                view_3d_->highlightCommunity(cid);
            }
        }

        int internal_node_count = 0;
        int bridge_node_count = 0;
        for (const auto& [nid, c] : communities_.assignment) {
            if (c.value == cid) {
                internal_node_count++;
                if (bridge_nodes_.count(nid)) bridge_node_count++;
            }
        }

        double internal_edge_count = 0;
        double external_edge_count = 0;
        std::set<uint32_t> linked_communities;

        for (const auto& [eid, edge] : graph_->getEdges()) {
            (void)eid;
            auto src_it = communities_.assignment.find(edge.source);
            auto tgt_it = communities_.assignment.find(edge.target);
            if (src_it == communities_.assignment.end() ||
                tgt_it == communities_.assignment.end()) {
                continue;
            }

            bool src_in = (src_it->second.value == cid);
            bool tgt_in = (tgt_it->second.value == cid);

            if (src_in && tgt_in) {
                internal_edge_count += edge.weight;
            } else if (src_in || tgt_in) {
                external_edge_count += edge.weight;
                uint32_t other_cid = src_in ? tgt_it->second.value : src_it->second.value;
                linked_communities.insert(other_cid);
            }
        }

        comm_nodes_label_->setText(QString::number(internal_node_count));
        comm_bridge_label_->setText(QString::number(bridge_node_count));
        comm_internal_edges_label_->setText(QString::number(internal_edge_count, 'f', 1));
        comm_external_edges_label_->setText(QString::number(external_edge_count, 'f', 1));
        comm_linked_label_->setText(QString::number(static_cast<int>(linked_communities.size())));
        update_neighbor_community_list(cid);
    }

    void on_node_clicked(satgraf::graph::NodeId id) {
        if (!graph_) return;

        auto node_opt = graph_->getNode(id);
        if (!node_opt) return;

        uint32_t cid = 0;
        bool has_community = false;
        auto cit = communities_.assignment.find(id);
        if (cit != communities_.assignment.end()) {
            cid = cit->second.value;
            has_community = true;
        }

        QColor color = rendering::community_color(cid);
        renderer_->set_decision_variable(id, coords_,
                                          static_cast<double>(node_size_slider_->value()));

        node_search_edit_->setText(QString::number(id.value));

        int internal_neighbors = 0;
        int external_neighbors = 0;
        std::set<uint32_t> linked_communities;

        if (has_community && !communities_.assignment.empty()) {
            const auto& node = node_opt->get();
            for (const auto& eid : node.edges) {
                auto edge_it = graph_->getEdges().find(eid);
                if (edge_it == graph_->getEdges().end()) continue;
                const auto& edge = edge_it->second;

                graph::NodeId neighbor =
                    (edge.source == id) ? edge.target : edge.source;

                auto neighbor_it = communities_.assignment.find(neighbor);
                if (neighbor_it == communities_.assignment.end()) continue;

                if (neighbor_it->second.value == cid) {
                    internal_neighbors++;
                } else {
                    external_neighbors++;
                    linked_communities.insert(neighbor_it->second.value);
                }
            }
        }

        QString community_str = has_community ? QString::number(cid) : QString("—");
        QString int_nbr_str = has_community ? QString::number(internal_neighbors) : QString("—");
        QString ext_nbr_str = has_community ? QString::number(external_neighbors) : QString("—");
        QString linked_str = has_community ? QString::number(static_cast<int>(linked_communities.size())) : QString("—");

        node_id_label_->setText(QString("%1 (%2)")
            .arg(id.value)
            .arg(QString::fromStdString(node_opt->get().name)));
        node_community_label_->setText(community_str);
        if (has_community) {
            QColor bg = rendering::community_color(cid);
            node_community_label_->setStyleSheet(
                QString("background-color: rgba(%1,%2,%3,80); padding: 1px 4px; border-radius: 2px;")
                    .arg(bg.red()).arg(bg.green()).arg(bg.blue()));
        } else {
            node_community_label_->setStyleSheet("");
        }
        node_int_nbr_label_->setText(int_nbr_str);
        node_ext_nbr_label_->setText(ext_nbr_str);
        node_linked_label_->setText(linked_str);

        uint32_t comm_for_clauses = has_community ? cid : UINT32_MAX;
        update_clause_display(id, comm_for_clauses);

        log(QString("Selected node %1, community %2").arg(id.value).arg(community_str));
    }

    void on_node_search() {
        if (!graph_) return;

        QString text = node_search_edit_->text().trimmed();
        if (text.isEmpty()) return;

        bool ok = false;
        uint32_t num = text.toUInt(&ok);
        if (ok) {
            graph::NodeId candidate{num};
            if (graph_->getNode(candidate)) {
                on_node_clicked(candidate);
                return;
            }
        }

        for (const auto& [nid, node] : graph_->getNodes()) {
            if (QString::fromStdString(node.name).startsWith(text, Qt::CaseInsensitive)) {
                on_node_clicked(nid);
                return;
            }
        }

        log("Node not found: " + text);
    }

    void load_cnf(const std::string& path) {
        open_file_path(QString::fromStdString(path));
    }

    void start_solver() {
        if (!graph_ || cnf_path_.isEmpty()) return;

        QString solver_path = solver_path_edit_->text().trimmed();
        if (solver_path.isEmpty()) {
            log("Set solver binary path first");
            return;
        }

        try {
            if (solver_timer_) { solver_timer_->stop(); delete solver_timer_; }
            engine_ = std::make_unique<evolution::EvolutionEngine>(*graph_);
            solver_.cancel();

            std::string fifo_path = solver::generate_fifo_path();
            fifo_.emplace(fifo_path);

            solver_.start(solver_path.toStdString(),
                          cnf_path_.toStdString(), fifo_path);
            solver_.open_fifo_for_read();

            start_solver_btn_->setText("■  Stop Solver");
            start_solver_btn_->disconnect();
            connect(start_solver_btn_, &QPushButton::clicked, this, [this] {
                solver_.cancel();
                if (solver_timer_) solver_timer_->stop();
                start_solver_btn_->setText("▶  Start Solver");
                start_solver_btn_->disconnect();
                connect(start_solver_btn_, &QPushButton::clicked, this, &MainWindow::start_solver);
                step_back_btn_->setEnabled(engine_ && engine_->history_depth() > 0);
                evolution_active_ = false;
                render_mode_combo_->setEnabled(true);
                log("Solver stopped");
            });

            step_back_btn_->setEnabled(false);
            timeline_slider_->setRange(0, 0);
            timeline_slider_->setEnabled(true);
            conflict_label_->setText("C: 0");

            solver_timer_ = new QTimer(this);
            connect(solver_timer_, &QTimer::timeout, this, &MainWindow::on_solver_timer);
            solver_timer_->start(100);

            evolution_active_ = true;
            render_mode_ = RenderMode::Detailed2D;
            render_mode_combo_->blockSignals(true);
            render_mode_combo_->setCurrentIndex(0);
            render_mode_combo_->blockSignals(false);
            render_mode_combo_->setEnabled(false);
            auto dit = mode_cache_.find(RenderMode::Detailed2D);
            if (dit == mode_cache_.end() && graph_ && !communities_.assignment.empty()) {
                int iters = iterations_slider_ ? iterations_slider_->value() : 20;
                auto entry = compute_mode_layout(RenderMode::Detailed2D, *graph_, communities_, quotient_graph_, iters);
                mode_cache_[RenderMode::Detailed2D] = std::move(entry);
                coords_ = mode_cache_[RenderMode::Detailed2D].coords;
            }
            if (!coords_.empty() && graph_) {
                perform_render();
            }

            log("Solver started");
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", e.what());
            log(QString("Solver error: %1").arg(e.what()));
        }
    }

    void on_solver_timer() {
        if (!solver_.running() && !solver_.is_running()) {
            solver_timer_->stop();
            start_solver_btn_->setText("▶  Start Solver");
            start_solver_btn_->disconnect();
            connect(start_solver_btn_, &QPushButton::clicked, this, &MainWindow::start_solver);
            evolution_active_ = false;
            render_mode_combo_->setEnabled(true);
            log("Solver finished");
            update_evolution_ui();
            return;
        }

        for (int i = 0; i < 50; ++i) {
            std::string line = solver_.read_fifo_line(std::chrono::milliseconds(10));
            if (line.empty()) break;
            engine_->process_line(line);
        }

        update_evolution_ui();
    }

    void evolution_step_backward() {
        if (!engine_ || !engine_->step_backward()) return;
        update_evolution_ui();
        re_render_evolution();
        log(QString("Step back — depth %1").arg(engine_->history_depth()));
    }

    void on_timeline_changed(int value) {
        if (!engine_) return;
        while (static_cast<int>(engine_->history_depth()) > value) {
            if (!engine_->step_backward()) break;
        }
        update_evolution_ui();
        re_render_evolution();
    }

private:
    struct ModeCacheEntry {
        layout::CoordinateMap coords;                      // Detailed2D: full graph 2D layout result
        std::vector<layout::Coordinate> community_centers; // Simple2D: community center positions
        layout::Coordinate3DMap coords_3d;                 // Mode3D: full graph 3D layout result
        layout::Coordinate3DMap community_centers_3d;      // Simple3D: community center 3D positions
    };

    bool has_cached_layout() const {
        auto it = mode_cache_.find(render_mode_);
        if (it == mode_cache_.end()) return false;
        switch (render_mode_) {
            case RenderMode::Detailed2D: return !it->second.coords.empty();
            case RenderMode::Simple2D:   return !it->second.community_centers.empty();
            case RenderMode::Mode3D:     return !it->second.coords_3d.empty();
            case RenderMode::Simple3D:   return !it->second.community_centers_3d.empty();
        }
        return false;
    }

    void update_layout_combo_for_mode() {
        if (!layout_combo_) return;
        layout_combo_->blockSignals(true);
        layout_combo_->clear();

        layout::LayoutMode lm;
        switch (render_mode_) {
            case RenderMode::Detailed2D: lm = layout::LayoutMode::Detailed2D; break;
            case RenderMode::Simple2D:   lm = layout::LayoutMode::Simple2D; break;
            case RenderMode::Mode3D:     lm = layout::LayoutMode::Mode3D; break;
            case RenderMode::Simple3D:   lm = layout::LayoutMode::Simple3D; break;
        }

        auto algorithms = layout::LayoutFactory::instance().available_algorithms(lm);
        for (const auto& name : algorithms) {
            layout_combo_->addItem(QString::fromStdString(name));
        }

        auto sel_it = mode_layout_selection_.find(static_cast<int>(render_mode_));
        if (sel_it != mode_layout_selection_.end()) {
            int idx = layout_combo_->findText(QString::fromStdString(sel_it->second));
            if (idx >= 0) layout_combo_->setCurrentIndex(idx);
        }

        layout_name_ = layout_combo_->currentText().toStdString();
        mode_layout_selection_[static_cast<int>(render_mode_)] = layout_name_;

        layout_combo_->blockSignals(false);
    }

    ModeCacheEntry compute_mode_layout(
        RenderMode mode,
        const graph::Graph<graph::Node, graph::Edge>& graph,
        const community::CommunityResult& communities,
        const layout::QuotientGraph& quotient_graph,
        int iterations)
    {
        ModeCacheEntry entry;

        auto progress = [this](double p) {
            if (render_cancel_.load()) return;
            int pct = static_cast<int>(p * 100);
            QMetaObject::invokeMethod(this, [this, pct] {
                log(QString("Layout: %1%").arg(pct));
            }, Qt::QueuedConnection);
        };

        switch (mode) {
            case RenderMode::Detailed2D: {
                auto layout = layout::LayoutFactory::instance().create(layout_name_);
                entry.coords = layout->compute(graph, progress);
                break;
            }
            case RenderMode::Simple2D: {
                satgraf::layout::CommunityWeightedLayout cwl(iterations);
                auto qcoords = cwl.compute(quotient_graph.graph, progress);

                entry.community_centers.reserve(quotient_graph.communities.size());
                for (std::size_t i = 0; i < quotient_graph.communities.size(); ++i) {
                    auto nid = graph::NodeId{static_cast<uint32_t>(i)};
                    auto it = qcoords.find(nid);
                    if (it != qcoords.end()) {
                        entry.community_centers.push_back(it->second);
                    } else {
                        entry.community_centers.push_back(layout::Coordinate{512.0, 512.0});
                    }
                }
                break;
            }
            case RenderMode::Mode3D: {
                satgraf::layout::ForceAtlas3DLayout fa3d(
                    static_cast<std::size_t>(iterations), 1024.0, 1024.0, 1024.0,
                    0.0, 10.0, 1.0, false, 1.0, 2);
                entry.coords_3d = fa3d.compute3D(graph);
                break;
            }
            case RenderMode::Simple3D: {
                satgraf::layout::CommunityWeighted3DLayout cw3d(iterations);
                entry.community_centers_3d = cw3d.compute3D(quotient_graph.graph, progress);
                break;
            }
        }

        return entry;
    }

    void setup_menus() {
        auto* file_menu = menuBar()->addMenu("File");
        file_menu->addAction("Open...", QKeySequence::Open, this, &MainWindow::open_file);
        file_menu->addSeparator();
        file_menu->addAction("Export...", QKeySequence::Save, this, &MainWindow::export_image);
        file_menu->addSeparator();
        file_menu->addAction("Quit", QKeySequence::Quit, this, &QWidget::close);

        auto* view_menu = menuBar()->addMenu("View");
        auto* menubar_action = view_menu->addAction("Show Menu Bar");
        menubar_action->setCheckable(true);
        menubar_action->setChecked(false);
        menubar_action->setShortcut(QKeySequence("Ctrl+M"));
        connect(menubar_action, &QAction::toggled, this, [this](bool on) {
            menuBar()->setVisible(on);
        });
        view_menu->addSeparator();
        view_menu->addAction("Zoom In", QKeySequence::ZoomIn, this, &MainWindow::zoom_in);
        view_menu->addAction("Zoom Out", QKeySequence::ZoomOut, this, &MainWindow::zoom_out);
        view_menu->addAction("Fit to View", this, &MainWindow::fit_to_view);

        auto* help_menu = menuBar()->addMenu("Help");
        help_menu->addAction("About", this, &MainWindow::show_about);
    }

    void setup_central_widget() {
        auto* central = new QWidget(this);
        central->setObjectName("central");
        central->setStyleSheet(
            "#central { background: palette(window); border-radius: 10px; }");
        auto* top_layout = new QVBoxLayout(central);
        top_layout->setContentsMargins(1, 1, 1, 1);
        top_layout->setSpacing(0);

        title_bar_ = new TitleBar(this, central);
        top_layout->addWidget(title_bar_);

        auto* main_splitter = new QSplitter(Qt::Horizontal, central);

        renderer_ = new rendering::GraphRenderer(this);
        view_ = new rendering::GraphView(renderer_->scene(), this);
        bg_brush_ = QBrush(Qt::black);
        view_->setBackgroundBrush(bg_brush_);
        view_->scene()->setBackgroundBrush(bg_brush_);
        connect(view_, &rendering::GraphView::node_clicked,
                this, &MainWindow::on_node_clicked);

        renderer_->set_community_click_callback([this](uint32_t cid) {
            for (int i = 0; i < community_select_combo_->count(); ++i) {
                if (community_select_combo_->itemData(i).toUInt() == cid) {
                    community_select_combo_->setCurrentIndex(i);
                    break;
                }
            }
        });

        view_3d_ = new GraphView3D(this);
        view_3d_->hide();
        connect(view_3d_, &GraphView3D::communityClicked,
                this, [this](uint32_t cid) {
            for (int i = 0; i < community_select_combo_->count(); ++i) {
                if (community_select_combo_->itemData(i).toUInt() == cid) {
                    community_select_combo_->setCurrentIndex(i);
                    break;
                }
            }
        });
        connect(view_3d_, &GraphView3D::nodeClicked,
                this, [this](uint32_t nid) {
            on_node_clicked(satgraf::graph::NodeId{nid});
        });

        auto* left_panel = build_left_panel();
        auto* right_panel = build_right_panel();

        main_splitter->addWidget(left_panel);
        main_splitter->addWidget(view_);
        main_splitter->addWidget(view_3d_);
        main_splitter->addWidget(right_panel);

        main_splitter->setStretchFactor(0, 0);
        main_splitter->setStretchFactor(1, 1);
        main_splitter->setStretchFactor(2, 1);
        main_splitter->setStretchFactor(3, 0);

        left_panel->setMinimumWidth(200);
        right_panel->setMinimumWidth(200);

        top_layout->addWidget(main_splitter, 1);

        setCentralWidget(central);
    }

    QWidget* build_left_panel() {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);

        file_label_ = new QLabel("No file loaded", this);
        file_label_->setWordWrap(true);
        layout->addWidget(file_label_);

        auto* open_btn = new QPushButton("Open File...", this);
        connect(open_btn, &QPushButton::clicked, this, &MainWindow::open_file);
        layout->addWidget(open_btn);

        layout->addWidget(separator());

        auto* mode_grid = new QFormLayout();
        mode_grid->setSpacing(4);
        mode_grid->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        mode_grid->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

        mode_grid->addRow("Render Mode:", [&] {
            auto* combo = new QComboBox(this);
            combo->addItem("2D", 0);
            combo->addItem("Simple 2D", 1);
            combo->addItem("3D", 2);
            combo->addItem("Simple 3D", 3);
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MainWindow::on_render_mode_changed);
            render_mode_combo_ = combo;
            return combo;
        }());

        mode_grid->addRow("Layout:", [&] {
            auto* combo = new QComboBox(this);
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::set_layout_algorithm);
            layout_combo_ = combo;
            return combo;
        }());

        mode_grid->addRow("Community:", [&] {
            auto* combo = new QComboBox(this);
            for (const auto& n : community::DetectorFactory::instance().available_algorithms())
                combo->addItem(QString::fromStdString(n));
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::set_community_method);
            community_combo_ = combo;
            return combo;
        }());

        layout->addLayout(mode_grid);

        mode_layout_selection_[0] = "f";
        mode_layout_selection_[1] = "community-fa";
        mode_layout_selection_[2] = "fa3d";
        mode_layout_selection_[3] = "community-fa3d";
        update_layout_combo_for_mode();

        add_form_row(layout, "Background:", [&] {
            auto* combo = new QComboBox(this);
            combo->addItems({"Dark", "White", "Light Gray", "Transparent"});
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::set_background);
            return combo;
        }());

        add_form_row(layout, "Iterations:", [&] {
            auto* row = new QWidget(this);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            auto* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(1, 2000);
            slider->setValue(20);
            auto* val = new QLabel("20", this);
            val->setFixedWidth(35);
            connect(slider, &QSlider::valueChanged, this, [val](int v) {
                val->setText(QString::number(v));
            });
            hl->addWidget(slider);
            hl->addWidget(val);
            iterations_slider_ = slider;
            return row;
        }());

        add_form_row(layout, "Node Size:", [&] {
            auto* row = new QWidget(this);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            auto* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(1, 25);
            slider->setValue(10);
            auto* val = new QLabel("10", this);
            val->setFixedWidth(25);
            connect(slider, &QSlider::valueChanged, this, [this, val](int v) {
                val->setText(QString::number(v));
                if (has_cached_layout()) {
                    perform_render();
                }
            });
            hl->addWidget(slider);
            hl->addWidget(val);
            node_size_slider_ = slider;
            return row;
        }());

        add_form_row(layout, "Edge Size:", [&] {
            auto* row = new QWidget(this);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            auto* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(1, 10);
            slider->setValue(1);
            auto* val = new QLabel("1", this);
            val->setFixedWidth(25);
            connect(slider, &QSlider::valueChanged, this, [this, val](int v) {
                val->setText(QString::number(v));
                if (has_cached_layout()) {
                    perform_render();
                }
            });
            hl->addWidget(slider);
            hl->addWidget(val);
            edge_size_slider_ = slider;
            return row;
        }());

        community_region_check_ = new QCheckBox("Show Community Regions", this);
        community_region_check_->setChecked(false);
        connect(community_region_check_, &QCheckBox::toggled, this, [this](bool on) {
            renderer_->set_community_regions_visible(on);
        });
        layout->addWidget(community_region_check_);

        show_labels_check_ = new QCheckBox("Show Labels (Simple)", this);
        show_labels_check_->setChecked(true);
        connect(show_labels_check_, &QCheckBox::toggled, this, [this] {
            if (!mode_cache_.empty()) perform_render();
        });
        layout->addWidget(show_labels_check_);

        layout->addWidget(separator());

        render_btn_ = new QPushButton("▶  Render", this);
        render_btn_->setMinimumHeight(36);
        render_btn_->setEnabled(false);
        connect(render_btn_, &QPushButton::clicked, this, &MainWindow::render_graph);
        layout->addWidget(render_btn_);

        layout->addWidget(separator());

        auto* evo_title = new QLabel("<b>Evolution</b>", this);
        layout->addWidget(evo_title);

        add_form_row(layout, "Solver:", [&] {
            auto* row = new QWidget(this);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(4);
            solver_path_edit_ = new QLineEdit(this);
            QSettings settings("satgraf", "satGraf");
            solver_path_edit_->setText(settings.value("solver/path").toString());
            solver_path_edit_->setPlaceholderText("Solver binary...");
            auto* browse_btn = new QPushButton("…", this);
            browse_btn->setFixedWidth(28);
            connect(browse_btn, &QPushButton::clicked, this, [this] {
                QString p = QFileDialog::getOpenFileName(
                    this, "Select Solver Binary", {});
                if (!p.isEmpty()) {
                    solver_path_edit_->setText(p);
                    QSettings("satgraf", "satGraf").setValue("solver/path", p);
                }
            });
            hl->addWidget(solver_path_edit_);
            hl->addWidget(browse_btn);
            return row;
        }());

        start_solver_btn_ = new QPushButton("▶  Start Solver", this);
        start_solver_btn_->setEnabled(false);
        connect(start_solver_btn_, &QPushButton::clicked, this, &MainWindow::start_solver);
        layout->addWidget(start_solver_btn_);

        add_form_row(layout, "Timeline:", [&] {
            auto* row = new QWidget(this);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(6);
            step_back_btn_ = new QPushButton("◀", this);
            step_back_btn_->setFixedWidth(28);
            step_back_btn_->setEnabled(false);
            connect(step_back_btn_, &QPushButton::clicked, this, &MainWindow::evolution_step_backward);
            auto* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(0, 0);
            slider->setEnabled(false);
            connect(slider, &QSlider::valueChanged, this, &MainWindow::on_timeline_changed);
            conflict_label_ = new QLabel("C: 0", this);
            conflict_label_->setFixedWidth(36);
            hl->addWidget(step_back_btn_);
            hl->addWidget(slider);
            hl->addWidget(conflict_label_);
            timeline_slider_ = slider;
            return row;
        }());

        layout->addStretch();
        return panel;
    }

    QWidget* build_right_panel() {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(6);

        auto* stats_title = new QLabel("<b>Statistics</b>", this);
        layout->addWidget(stats_title);

        auto* stats_grid = new QFormLayout();
        stats_grid->setSpacing(4);
        stats_grid->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        stats_nodes_ = new QLabel("0", this);
        stats_nodes_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_edges_ = new QLabel("0", this);
        stats_edges_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_communities_ = new QLabel("—", this);
        stats_communities_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_modularity_ = new QLabel("—", this);
        stats_modularity_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_grid->addRow("Nodes:", stats_nodes_);
        stats_grid->addRow("Edges:", stats_edges_);
        stats_grid->addRow("Communities:", stats_communities_);
        stats_grid->addRow("Modularity:", stats_modularity_);
        stats_internal_edges_ = new QLabel("—", this);
        stats_internal_edges_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_external_edges_ = new QLabel("—", this);
        stats_external_edges_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        stats_grid->addRow("Internal Edges:", stats_internal_edges_);
        stats_grid->addRow("External Edges:", stats_external_edges_);
        layout->addLayout(stats_grid);

        layout->addWidget(separator());

        comm_content_ = new QWidget(this);
        auto* comm_inner = new QVBoxLayout(comm_content_);
        comm_inner->setContentsMargins(0, 0, 0, 0);
        comm_inner->setSpacing(4);

        community_select_combo_ = new QComboBox(this);
        community_select_combo_->addItem("— none —", 0xFFFFFFFF);
        connect(community_select_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::on_community_selected);
        comm_inner->addWidget(community_select_combo_);

        highlight_check_ = new QCheckBox("Highlight", this);
        highlight_check_->setChecked(true);
        connect(highlight_check_, &QCheckBox::toggled, this, [this]() {
            if (selected_community_ && highlight_check_->isChecked()) {
                renderer_->highlight_community(selected_community_);
                renderer_->highlight_simple2d_community(selected_community_);
                if (render_mode_ == RenderMode::Mode3D && view_3d_ && view_3d_->isVisible()) {
                    view_3d_->highlightCommunity(selected_community_);
                }
            } else {
                renderer_->highlight_community(std::nullopt);
                renderer_->highlight_simple2d_community(std::nullopt);
                if (view_3d_) view_3d_->highlightCommunity(std::nullopt);
            }
        });
        comm_inner->addWidget(highlight_check_);

        auto* comm_grid = new QFormLayout();
        comm_grid->setSpacing(4);
        comm_grid->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        comm_nodes_label_ = new QLabel("—", this);
        comm_nodes_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        comm_bridge_label_ = new QLabel("—", this);
        comm_bridge_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        comm_internal_edges_label_ = new QLabel("—", this);
        comm_internal_edges_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        comm_external_edges_label_ = new QLabel("—", this);
        comm_external_edges_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        comm_linked_label_ = new QLabel("—", this);
        comm_linked_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        comm_grid->addRow("Nodes:", comm_nodes_label_);
        comm_grid->addRow("Bridge Nodes:", comm_bridge_label_);
        comm_grid->addRow("Internal Edges:", comm_internal_edges_label_);
        comm_grid->addRow("External Edges:", comm_external_edges_label_);
        comm_grid->addRow("Linked Communities:", comm_linked_label_);
        comm_inner->addLayout(comm_grid);

        auto* neighbor_label = new QLabel("<b>Neighbors</b>", this);
        comm_inner->addWidget(neighbor_label);

        auto* neighbor_scroll = new QScrollArea(this);
        neighbor_scroll->setWidgetResizable(true);
        neighbor_scroll->setMaximumHeight(120);
        neighbor_list_container_ = new QWidget(this);
        neighbor_list_layout_ = new QVBoxLayout(neighbor_list_container_);
        neighbor_list_layout_->setContentsMargins(0, 0, 0, 0);
        neighbor_list_layout_->setSpacing(2);
        neighbor_list_layout_->addStretch();
        neighbor_scroll->setWidget(neighbor_list_container_);
        comm_inner->addWidget(neighbor_scroll);

        comm_toggle_ = make_collapsible_section("Community", comm_content_);
        layout->addWidget(comm_toggle_);
        layout->addWidget(comm_content_);

        layout->addWidget(separator());

        var_content_ = new QWidget(this);
        auto* var_inner = new QVBoxLayout(var_content_);
        var_inner->setContentsMargins(0, 0, 0, 0);
        var_inner->setSpacing(4);

        node_search_edit_ = new QLineEdit(this);
        node_search_edit_->setPlaceholderText("Search node by ID...");
        node_search_edit_->setEnabled(false);
        connect(node_search_edit_, &QLineEdit::returnPressed,
                this, &MainWindow::on_node_search);
        var_inner->addWidget(node_search_edit_);

        auto* node_grid = new QFormLayout();
        node_grid->setSpacing(4);
        node_grid->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        node_id_label_ = new QLabel("—", this);
        node_id_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        node_community_label_ = new QLabel("—", this);
        node_community_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        node_int_nbr_label_ = new QLabel("—", this);
        node_int_nbr_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        node_ext_nbr_label_ = new QLabel("—", this);
        node_ext_nbr_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        node_linked_label_ = new QLabel("—", this);
        node_linked_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        node_grid->addRow("Node:", node_id_label_);
        node_grid->addRow("Community:", node_community_label_);
        node_grid->addRow("Internal Neighbors:", node_int_nbr_label_);
        node_grid->addRow("External Neighbors:", node_ext_nbr_label_);
        node_grid->addRow("Linked Communities:", node_linked_label_);
        var_inner->addLayout(node_grid);

        auto* clause_label = new QLabel("<b>Clauses</b>", this);
        var_inner->addWidget(clause_label);

        clause_display_ = new QWidget(this);
        auto* clause_vlayout = new QVBoxLayout(clause_display_);
        clause_vlayout->setContentsMargins(0, 0, 0, 0);
        clause_vlayout->setSpacing(8);

        auto* pos_section = new QWidget(this);
        auto* pos_section_layout = new QVBoxLayout(pos_section);
        pos_section_layout->setContentsMargins(0, 0, 0, 0);
        pos_section_layout->setSpacing(2);
        pos_section_layout->addWidget(new QLabel("<b>Pos Clauses</b>", this));
        auto* pos_scroll = new QScrollArea(this);
        pos_scroll->setWidgetResizable(true);
        pos_scroll->setMaximumHeight(300);
        auto* pos_inner_widget = new QWidget(this);
        pos_clause_layout_ = new QVBoxLayout(pos_inner_widget);
        pos_clause_layout_->setContentsMargins(0, 0, 0, 0);
        pos_clause_layout_->setSpacing(4);
        pos_clause_layout_->addStretch();
        pos_scroll->setWidget(pos_inner_widget);
        pos_section_layout->addWidget(pos_scroll);
        clause_vlayout->addWidget(pos_section);

        auto* neg_section = new QWidget(this);
        auto* neg_section_layout = new QVBoxLayout(neg_section);
        neg_section_layout->setContentsMargins(0, 0, 0, 0);
        neg_section_layout->setSpacing(2);
        neg_section_layout->addWidget(new QLabel("<b>Neg Clauses</b>", this));
        auto* neg_scroll = new QScrollArea(this);
        neg_scroll->setWidgetResizable(true);
        neg_scroll->setMaximumHeight(300);
        auto* neg_inner_widget = new QWidget(this);
        neg_clause_layout_ = new QVBoxLayout(neg_inner_widget);
        neg_clause_layout_->setContentsMargins(0, 0, 0, 0);
        neg_clause_layout_->setSpacing(4);
        neg_clause_layout_->addStretch();
        neg_scroll->setWidget(neg_inner_widget);
        neg_section_layout->addWidget(neg_scroll);
        clause_vlayout->addWidget(neg_section);

        var_inner->addWidget(clause_display_);

        var_toggle_ = make_collapsible_section("Variable", var_content_);
        layout->addWidget(var_toggle_);
        layout->addWidget(var_content_);

        layout->addStretch();
        layout->addWidget(separator());

        log_label_ = new QLabel("Ready", this);
        log_label_->setWordWrap(true);
        log_label_->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
        layout->addWidget(log_label_);
        return panel;
    }

    QFrame* separator() {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        return line;
    }

    void add_form_row(QVBoxLayout* layout, const QString& label, QWidget* widget) {
        auto* lbl = new QLabel(label, this);
        layout->addWidget(lbl);
        layout->addWidget(widget);
    }

    QToolButton* make_collapsible_section(const QString& title, QWidget* content) {
        auto* btn = new QToolButton(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setArrowType(Qt::DownArrow);
        btn->setText(title);
        btn->setCheckable(true);
        btn->setChecked(false);
        btn->setStyleSheet("QToolButton { font-weight: bold; border: none; padding: 4px 0; }");
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(btn, &QToolButton::toggled, this, [btn, content](bool checked) {
            content->setVisible(!checked);
            btn->setArrowType(checked ? Qt::RightArrow : Qt::DownArrow);
        });
        content->setVisible(true);
        return btn;
    }

    void clear_layout(QLayout* layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            if (item->layout()) clear_layout(item->layout());
            delete item;
        }
    }

    void update_neighbor_community_list(uint32_t selected_cid) {
        if (!neighbor_list_layout_ || !graph_) return;

        clear_layout(neighbor_list_layout_);

        struct NeighborInfo {
            uint32_t cid;
            double edge_count;
        };
        std::map<uint32_t, double> neighbor_edge_counts;

        for (const auto& [eid, edge] : graph_->getEdges()) {
            (void)eid;
            auto src_it = communities_.assignment.find(edge.source);
            auto tgt_it = communities_.assignment.find(edge.target);
            if (src_it == communities_.assignment.end() || tgt_it == communities_.assignment.end()) continue;
            auto src_cid = src_it->second.value;
            auto tgt_cid = tgt_it->second.value;
            if (src_cid == tgt_cid) continue;
            if (src_cid == selected_cid) {
                neighbor_edge_counts[tgt_cid] += edge.weight;
            } else if (tgt_cid == selected_cid) {
                neighbor_edge_counts[src_cid] += edge.weight;
            }
        }

        std::vector<NeighborInfo> neighbors;
        for (const auto& [cid, count] : neighbor_edge_counts) {
            neighbors.push_back({cid, count});
        }

        std::sort(neighbors.begin(), neighbors.end(),
                  [](const auto& a, const auto& b) { return a.edge_count > b.edge_count; });

        if (neighbors.empty()) {
            auto* lbl = new QLabel("(no neighbors)", neighbor_list_container_);
            lbl->setAlignment(Qt::AlignCenter);
            neighbor_list_layout_->addWidget(lbl);
        } else {
            for (const auto& n : neighbors) {
                auto* row = new QWidget(neighbor_list_container_);
                auto* hl = new QHBoxLayout(row);
                hl->setContentsMargins(0, 0, 0, 0);
                hl->setSpacing(6);

                QColor color = rendering::community_color(n.cid);
                auto* swatch = new QLabel(neighbor_list_container_);
                QPixmap pix(14, 14);
                pix.fill(color);
                swatch->setPixmap(pix);
                swatch->setFixedSize(14, 14);
                hl->addWidget(swatch);

                auto* id_lbl = new QLabel(QString("%1").arg(n.cid), neighbor_list_container_);
                hl->addWidget(id_lbl, 1);

                auto* count_lbl = new QLabel(QString::number(n.edge_count, 'f', 1), neighbor_list_container_);
                count_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                hl->addWidget(count_lbl);

                neighbor_list_layout_->addWidget(row);
            }
        }
        neighbor_list_layout_->addStretch();
    }

    void update_clause_display(satgraf::graph::NodeId selected_node, uint32_t selected_community) {
        if (!pos_clause_layout_ || !neg_clause_layout_ || !graph_) return;

        clear_layout(pos_clause_layout_);
        clear_layout(neg_clause_layout_);

        std::vector<std::pair<std::reference_wrapper<const graph::Clause>, bool>> pos_clauses, neg_clauses;

        for (const auto& clause : graph_->getClauses()) {
            auto it = clause.literals().find(selected_node);
            if (it == clause.literals().end()) continue;
            if (it->second) {
                pos_clauses.emplace_back(std::cref(clause), true);
            } else {
                neg_clauses.emplace_back(std::cref(clause), false);
            }
        }

        auto sort_by_size = [](auto& vec) {
            std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
                return a.first.get().size() < b.first.get().size();
            });
        };
        sort_by_size(pos_clauses);
        sort_by_size(neg_clauses);

        auto render_column = [&](QVBoxLayout* col_layout, 
                                  std::vector<std::pair<std::reference_wrapper<const graph::Clause>, bool>>& clauses) {
            if (clauses.empty()) {
                col_layout->addWidget(new QLabel("(none)", clause_display_));
                col_layout->addStretch();
                return;
            }

            for (const auto& [clause_ref, polarity] : clauses) {
                const auto& clause = clause_ref.get();

                auto* row = new QWidget(clause_display_);
                auto* hl = new QHBoxLayout(row);
                hl->setContentsMargins(2, 1, 2, 1);
                hl->setSpacing(2);

                for (const auto& [lit_node, lit_polarity] : clause.literals()) {
                    QString text = lit_polarity
                        ? QString("+%1").arg(lit_node.value)
                        : QString("-%1").arg(lit_node.value);
                    auto* lit_lbl = new QLabel(text, clause_display_);

                    if (selected_community != UINT32_MAX) {
                        auto cit = communities_.assignment.find(lit_node);
                        if (cit != communities_.assignment.end() && cit->second.value != selected_community) {
                            QColor bg = rendering::community_color(cit->second.value);
                            lit_lbl->setStyleSheet(
                                QString("background-color: rgba(%1,%2,%3,80); padding: 1px 3px; border-radius: 2px;")
                                    .arg(bg.red()).arg(bg.green()).arg(bg.blue()));
                        }
                    }

                    hl->addWidget(lit_lbl);
                }
                col_layout->addWidget(row);
            }

            col_layout->addStretch();
        };

        render_column(pos_clause_layout_, pos_clauses);
        render_column(neg_clause_layout_, neg_clauses);
    }

    void log(const QString& msg) {
        if (log_label_) log_label_->setText(msg);
        QApplication::processEvents();
    }

    void populate_community_combo() {
        community_select_combo_->blockSignals(true);
        community_select_combo_->clear();
        community_select_combo_->addItem("— none —", 0xFFFFFFFF);

        std::set<satgraf::graph::CommunityId> unique;
        for (const auto& [nid, cid] : communities_.assignment) {
            (void)nid;
            unique.insert(cid);
        }

        for (const auto& cid : unique) {
            QColor color = rendering::community_color(cid.value);
            QPixmap pix(14, 14);
            pix.fill(color);
            community_select_combo_->addItem(
                QIcon(pix), QString("Community %1").arg(cid.value),
                QVariant(cid.value));
        }
        community_select_combo_->blockSignals(false);
    }

    void update_evolution_ui() {
        if (!engine_) return;
        conflict_label_->setText(QString("C: %1").arg(engine_->current_conflict()));

        timeline_slider_->blockSignals(true);
        timeline_slider_->setRange(0, static_cast<int>(engine_->history_depth()));
        timeline_slider_->setValue(static_cast<int>(engine_->history_depth()));
        timeline_slider_->blockSignals(false);

        step_back_btn_->setEnabled(engine_->history_depth() > 0);
    }

    void re_render_evolution() {
        if (!graph_ || coords_.empty()) return;
        double node_size = node_size_slider_ ? node_size_slider_->value() : 10;
        double edge_size = edge_size_slider_ ? edge_size_slider_->value() : 1.0;
        renderer_->render(*graph_, coords_, communities_, node_size, edge_size);
        view_->scene()->setBackgroundBrush(bg_brush_);
        view_->setBackgroundBrush(bg_brush_);
        renderer_->add_labels(*graph_);
        renderer_->set_scene_rect(renderer_->compute_scene_rect(coords_));

        if (engine_) {
            auto dv = engine_->decision_variable();
            if (dv) {
                renderer_->set_decision_variable(*dv, coords_, node_size);
            }
        }

        if (selected_community_) {
            renderer_->highlight_community(selected_community_);
        }
    }

    void on_render_complete(uint64_t gen,
                            satgraf::community::CommunityResult communities,
                            std::unordered_set<satgraf::graph::NodeId> bridge_nodes,
                            satgraf::layout::QuotientGraph quotient_graph,
                            std::vector<std::size_t> community_sizes,
                            std::vector<uint32_t> community_ids,
                            std::map<std::pair<uint32_t, uint32_t>, double> inter_community_edges,
                            ModeCacheEntry mode_entry) {
        if (gen != render_generation_.load()) return;
        if (render_cancel_.load()) return;

        communities_ = std::move(communities);
        bridge_nodes_ = std::move(bridge_nodes);
        quotient_graph_ = std::move(quotient_graph);
        community_sizes_ = std::move(community_sizes);
        community_ids_ = std::move(community_ids);
        inter_community_edges_ = std::move(inter_community_edges);

        mode_cache_[render_mode_] = std::move(mode_entry);
        if (render_mode_ == RenderMode::Detailed2D) {
            coords_ = mode_cache_[RenderMode::Detailed2D].coords;
        }

        std::set<satgraf::graph::CommunityId> unique;
        for (const auto& [nid, cid] : communities_.assignment) {
            (void)nid;
            unique.insert(cid);
        }
        stats_communities_->setText(QString::number(unique.size()));
        stats_modularity_->setText(QString::number(communities_.q_modularity, 'f', 3));
        stats_internal_edges_->setText(QString::number(communities_.intra_edges));
        stats_external_edges_->setText(QString::number(communities_.inter_edges));

        perform_render();
        populate_community_combo();

        rendering_active_.store(false);
        render_btn_->setText("▶  Render");
        set_controls_enabled(true);
        node_search_edit_->setEnabled(true);
        log("Render complete");
    }

    void perform_render() {
        double node_size = node_size_slider_ ? node_size_slider_->value() : 10;
        double edge_size = edge_size_slider_ ? edge_size_slider_->value() : 1.0;

        double avg_community_size = community_sizes_.empty() ? 1.0
            : std::accumulate(community_sizes_.begin(), community_sizes_.end(), 0.0)
              / static_cast<double>(community_sizes_.size());

        switch (render_mode_) {
            case RenderMode::Detailed2D: {
                auto it = mode_cache_.find(RenderMode::Detailed2D);
                if (it == mode_cache_.end() || it->second.coords.empty()) break;
                const auto& c = it->second.coords;
                renderer_->render(*graph_, c, communities_, node_size, edge_size);
                renderer_->store_graph(graph_.get());
                view_->scene()->setBackgroundBrush(bg_brush_);
                view_->setBackgroundBrush(bg_brush_);
                renderer_->add_labels(*graph_);
                renderer_->set_scene_rect(renderer_->compute_scene_rect(c));
                if (view_) view_->fit_all();
                renderer_->set_community_regions_visible(community_region_check_->isChecked());
                if (highlight_check_->isChecked() && selected_community_) {
                    renderer_->highlight_community(selected_community_);
                }
                view_3d_->hide();
                view_->show();
                break;
            }
            case RenderMode::Simple2D: {
                auto it = mode_cache_.find(RenderMode::Simple2D);
                if (it == mode_cache_.end() || it->second.community_centers.empty()) break;
                perform_simple_2d_render(it->second.community_centers, node_size, edge_size, avg_community_size);
                if (highlight_check_->isChecked() && selected_community_) {
                    renderer_->highlight_simple2d_community(selected_community_);
                }
                view_3d_->hide();
                view_->show();
                break;
            }
            case RenderMode::Mode3D: {
                auto it = mode_cache_.find(RenderMode::Mode3D);
                if (it == mode_cache_.end() || it->second.coords_3d.empty()) {
                    view_3d_->hide();
                    view_->show();
                    break;
                }
                const auto& coords_3d = it->second.coords_3d;

                std::vector<QColor> palette;
                for (std::size_t i = 0; i < 20; ++i) {
                    palette.push_back(rendering::community_color(
                        static_cast<uint32_t>(i)));
                }

                float radius = 0.10f * static_cast<float>(node_size / 10.0);
                view_3d_->setFullGraph(
                    coords_3d, communities_.assignment, palette, radius, graph_.get(),
                    static_cast<float>(edge_size));
                view_3d_->highlightCommunity(selected_community_);

                view_->hide();
                view_3d_->show();
                break;
            }
            case RenderMode::Simple3D: {
                auto it = mode_cache_.find(RenderMode::Simple3D);
                if (it == mode_cache_.end() || it->second.community_centers_3d.empty()) {
                    view_3d_->hide();
                    view_->show();
                    break;
                }
                const auto& cc3d = it->second.community_centers_3d;

                std::unordered_map<uint32_t, QColor> community_colors;
                for (auto cid : community_ids_) {
                    community_colors[cid] = rendering::community_color(cid);
                }

                view_3d_->setGraph(
                    cc3d, community_sizes_, community_ids_,
                    inter_community_edges_, community_colors,
                    static_cast<float>(node_size), avg_community_size,
                    static_cast<float>(edge_size),
                    show_labels_check_ ? show_labels_check_->isChecked() : true);

                view_->hide();
                view_3d_->show();
                break;
            }
        }
    }

    void perform_simple_2d_render(const std::vector<satgraf::layout::Coordinate>& community_centers,
                                   double node_size, double edge_size, double avg_community_size) {
        if (!graph_ || community_centers.empty()) return;

        std::unordered_map<uint32_t, QColor> community_colors;
        for (auto cid : community_ids_) {
            community_colors[cid] = rendering::community_color(cid);
        }

        bool show_labels = show_labels_check_ ? show_labels_check_->isChecked() : true;
        renderer_->render_simple_2d(
            community_centers, community_sizes_, community_ids_,
            inter_community_edges_, community_colors, node_size, avg_community_size, edge_size,
            show_labels);

        view_->scene()->setBackgroundBrush(bg_brush_);
        view_->setBackgroundBrush(bg_brush_);

        layout::CoordinateMap center_map;
        for (std::size_t i = 0; i < community_centers.size(); ++i) {
            center_map[graph::NodeId{static_cast<uint32_t>(i)}] = community_centers[i];
        }
        renderer_->set_scene_rect(renderer_->compute_scene_rect(center_map));
        if (view_) view_->fit_all();
    }

    using Graph = graph::Graph<graph::Node, graph::Edge>;

    rendering::GraphRenderer* renderer_{nullptr};
    rendering::GraphView* view_{nullptr};
    GraphView3D* view_3d_{nullptr};
    QBrush bg_brush_{Qt::black};
    TitleBar* title_bar_{nullptr};

    QLabel* file_label_{nullptr};
    QPushButton* render_btn_{nullptr};
    QComboBox* community_combo_{nullptr};
    QComboBox* layout_combo_{nullptr};
    QSlider* iterations_slider_{nullptr};
    QSlider* node_size_slider_{nullptr};
    QSlider* edge_size_slider_{nullptr};
    QCheckBox* community_region_check_{nullptr};
    QCheckBox* show_labels_check_{nullptr};

    QComboBox* community_select_combo_{nullptr};
    QCheckBox* highlight_check_{nullptr};
    QLabel* comm_nodes_label_{nullptr};
    QLabel* comm_bridge_label_{nullptr};
    QLabel* comm_internal_edges_label_{nullptr};
    QLabel* comm_external_edges_label_{nullptr};
    QLabel* comm_linked_label_{nullptr};
    QLineEdit* node_search_edit_{nullptr};
    QLabel* node_id_label_{nullptr};
    QLabel* node_community_label_{nullptr};
    QLabel* node_int_nbr_label_{nullptr};
    QLabel* node_ext_nbr_label_{nullptr};
    QLabel* node_linked_label_{nullptr};
    QLabel* log_label_{nullptr};

    QToolButton* comm_toggle_{nullptr};
    QWidget* comm_content_{nullptr};
    QToolButton* var_toggle_{nullptr};
    QWidget* var_content_{nullptr};

    QWidget* neighbor_list_container_{nullptr};
    QVBoxLayout* neighbor_list_layout_{nullptr};

    QWidget* clause_display_{nullptr};
    QVBoxLayout* pos_clause_layout_{nullptr};
    QVBoxLayout* neg_clause_layout_{nullptr};

    QLabel* stats_nodes_{nullptr};
    QLabel* stats_edges_{nullptr};
    QLabel* stats_communities_{nullptr};
    QLabel* stats_modularity_{nullptr};
    QLabel* stats_internal_edges_{nullptr};
    QLabel* stats_external_edges_{nullptr};

    std::unique_ptr<Graph> graph_;
    community::CommunityResult communities_;
    layout::CoordinateMap coords_;
    std::string layout_name_{"f"};
    std::string community_method_{"louvain"};
    std::unordered_set<satgraf::graph::NodeId> bridge_nodes_;
    std::optional<uint32_t> selected_community_;

    RenderMode render_mode_{RenderMode::Detailed2D};
    QComboBox* render_mode_combo_{nullptr};
    bool evolution_active_{false};

    satgraf::layout::QuotientGraph quotient_graph_;
    std::vector<std::size_t> community_sizes_;
    std::vector<uint32_t> community_ids_;
    std::map<std::pair<uint32_t, uint32_t>, double> inter_community_edges_;

    std::unordered_map<RenderMode, ModeCacheEntry> mode_cache_;
    std::unordered_map<int, std::string> mode_layout_selection_;

    QString cnf_path_;
    solver::ExternalSolver solver_;
    std::optional<solver::NamedFifo> fifo_;
    std::unique_ptr<evolution::EvolutionEngine> engine_;
    QTimer* solver_timer_{nullptr};

    QLineEdit* solver_path_edit_{nullptr};
    QPushButton* start_solver_btn_{nullptr};
    QPushButton* step_back_btn_{nullptr};
    QSlider* timeline_slider_{nullptr};
    QLabel* conflict_label_{nullptr};

    std::atomic<bool> rendering_active_{false};
    std::atomic<bool> render_cancel_{false};
    std::optional<std::thread> render_thread_;
    std::atomic<uint64_t> render_generation_{0};
};

}  // namespace satgraf::gui

Q_DECLARE_METATYPE(satgraf::community::CommunityResult)
Q_DECLARE_METATYPE(satgraf::layout::CoordinateMap)
