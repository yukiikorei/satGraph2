#pragma once

#include <satgraf/dimacs_parser.hpp>
#include <satgraf/graph.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/layout.hpp>
#include <satgraf/evolution.hpp>
#include <satgraf/solver.hpp>
#include <satgraf_gui/graph_renderer.hpp>
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
#include <string>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <atomic>

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

        set_controls_enabled(false);
        rendering_active_.store(true);
        render_cancel_.store(false);
        render_btn_->setText("⏸  Pause");
        render_btn_->setEnabled(true);
        log("Detecting communities...");

        std::thread([this] {
            try {
                auto detector = community::DetectorFactory::instance().create(community_method_);
                auto communities = detector->detect(*graph_);

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                std::set<satgraf::graph::CommunityId> unique;
                for (const auto& [nid, cid] : communities.assignment) {
                    (void)nid;
                    unique.insert(cid);
                }

                int iters = iterations_slider_ ? iterations_slider_->value() : 500;
                auto layout = layout::LayoutFactory::instance().create(layout_name_);
                auto coords = layout->compute(*graph_,
                    [this](double p) {
                        if (render_cancel_.load()) return;
                        int pct = static_cast<int>(p * 100);
                        QMetaObject::invokeMethod(this, [this, pct] {
                            log(QString("Layout: %1%").arg(pct));
                        }, Qt::QueuedConnection);
                    });

                if (render_cancel_.load()) {
                    QMetaObject::invokeMethod(this, "on_render_cancelled", Qt::QueuedConnection);
                    return;
                }

                QMetaObject::invokeMethod(this, "on_render_complete",
                    Qt::QueuedConnection,
                    Q_ARG(satgraf::community::CommunityResult, communities),
                    Q_ARG(satgraf::layout::CoordinateMap, coords));
            } catch (const std::exception& e) {
                QMetaObject::invokeMethod(this, "on_render_error",
                    Qt::QueuedConnection, Q_ARG(QString, QString(e.what())));
            }
        }).detach();
    }

    void on_render_complete(satgraf::community::CommunityResult communities,
                            satgraf::layout::CoordinateMap coords) {
        if (render_cancel_.load()) return;
        communities_ = std::move(communities);
        coords_ = std::move(coords);

        std::set<satgraf::graph::CommunityId> unique;
        for (const auto& [nid, cid] : communities_.assignment) {
            (void)nid;
            unique.insert(cid);
        }
        stats_communities_->setText(QString::number(unique.size()));
        stats_modularity_->setText(QString::number(communities_.q_modularity, 'f', 3));

        double node_size = node_size_slider_ ? node_size_slider_->value() : 10;
        renderer_->render(*graph_, coords_, communities_, node_size, 1.0);
        view_->scene()->setBackgroundBrush(bg_brush_);
        view_->setBackgroundBrush(bg_brush_);
        renderer_->add_labels(*graph_);
        renderer_->set_scene_rect(renderer_->compute_scene_rect(coords_));
        if (view_) view_->fit_all();

        renderer_->set_community_regions_visible(community_region_check_->isChecked());

        populate_community_combo();

        rendering_active_.store(false);
        render_btn_->setText("▶  Render");
        set_controls_enabled(true);
        log("Render complete");
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
        iterations_slider_->setEnabled(enabled);
        node_size_slider_->setEnabled(enabled);
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
    }

    void set_community_method(const QString& name) {
        community_method_ = name.toStdString();
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
    }

    void on_community_selected(int index) {
        if (index < 0 || !graph_) return;
        uint32_t cid = static_cast<uint32_t>(community_select_combo_->itemData(index).toUInt());

        QColor color = rendering::community_color(cid);
        int count = 0;
        for (const auto& [nid, c] : communities_.assignment) {
            (void)nid;
            if (c.value == cid) count++;
        }

        community_detail_->setText(
            QString("Community %1\nColor: %2\nNodes: %3")
                .arg(cid)
                .arg(color.name())
                .arg(count));
    }

    void on_node_clicked(satgraf::graph::NodeId id) {
        if (!graph_) return;

        auto node_opt = graph_->getNode(id);
        if (!node_opt) return;

        uint32_t cid = 0;
        auto cit = communities_.assignment.find(id);
        if (cit != communities_.assignment.end()) cid = cit->second.value;

        QColor color = rendering::community_color(cid);
        renderer_->set_decision_variable(id, coords_,
                                          static_cast<double>(node_size_slider_->value()));

        node_detail_->setText(
            QString("Node %1\nCommunity: %2\nColor: %3")
                .arg(id.value).arg(cid).arg(color.name()));

        log(QString("Selected node %1, community %2").arg(id.value).arg(cid));
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
                log("Solver stopped");
            });

            step_back_btn_->setEnabled(false);
            timeline_slider_->setRange(0, 0);
            timeline_slider_->setEnabled(true);
            conflict_label_->setText("C: 0");

            solver_timer_ = new QTimer(this);
            connect(solver_timer_, &QTimer::timeout, this, &MainWindow::on_solver_timer);
            solver_timer_->start(100);

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

        auto* left_panel = build_left_panel();
        auto* right_panel = build_right_panel();

        main_splitter->addWidget(left_panel);
        main_splitter->addWidget(view_);
        main_splitter->addWidget(right_panel);

        main_splitter->setStretchFactor(0, 0);
        main_splitter->setStretchFactor(1, 1);
        main_splitter->setStretchFactor(2, 0);

        left_panel->setFixedWidth(256);
        right_panel->setFixedWidth(230);

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

        add_form_row(layout, "Community:", [&] {
            auto* combo = new QComboBox(this);
            for (const auto& n : community::DetectorFactory::instance().available_algorithms())
                combo->addItem(QString::fromStdString(n));
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::set_community_method);
            community_combo_ = combo;
            return combo;
        }());

        add_form_row(layout, "Layout:", [&] {
            auto* combo = new QComboBox(this);
            for (const auto& n : layout::LayoutFactory::instance().available_algorithms())
                combo->addItem(QString::fromStdString(n));
            connect(combo, &QComboBox::currentTextChanged, this, &MainWindow::set_layout_algorithm);
            layout_combo_ = combo;
            return combo;
        }());

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
            slider->setRange(50, 2000);
            slider->setValue(500);
            auto* val = new QLabel("500", this);
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
            slider->setRange(2, 40);
            slider->setValue(10);
            auto* val = new QLabel("10", this);
            val->setFixedWidth(25);
            connect(slider, &QSlider::valueChanged, this, [this, val](int v) {
                val->setText(QString::number(v));
                if (!coords_.empty()) {
                    renderer_->render(*graph_, coords_, communities_,
                                      static_cast<double>(v), 1.0);
                    renderer_->add_labels(*graph_);
                    renderer_->set_scene_rect(
                        renderer_->compute_scene_rect(coords_));
                }
            });
            hl->addWidget(slider);
            hl->addWidget(val);
            node_size_slider_ = slider;
            return row;
        }());

        community_region_check_ = new QCheckBox("Show Community Regions", this);
        community_region_check_->setChecked(false);
        connect(community_region_check_, &QCheckBox::toggled, this, [this](bool on) {
            renderer_->set_community_regions_visible(on);
        });
        layout->addWidget(community_region_check_);

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
        stats_nodes_ = new QLabel("0", this);
        stats_edges_ = new QLabel("0", this);
        stats_communities_ = new QLabel("—", this);
        stats_modularity_ = new QLabel("—", this);
        stats_grid->addRow("Nodes:", stats_nodes_);
        stats_grid->addRow("Edges:", stats_edges_);
        stats_grid->addRow("Communities:", stats_communities_);
        stats_grid->addRow("Modularity:", stats_modularity_);
        layout->addLayout(stats_grid);

        layout->addWidget(separator());

        auto* comm_title = new QLabel("<b>Community</b>", this);
        layout->addWidget(comm_title);

        community_select_combo_ = new QComboBox(this);
        community_select_combo_->addItem("— none —", 0xFFFFFFFF);
        connect(community_select_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::on_community_selected);
        layout->addWidget(community_select_combo_);

        community_detail_ = new QLabel("", this);
        community_detail_->setWordWrap(true);
        community_detail_->setMinimumHeight(50);
        layout->addWidget(community_detail_);

        layout->addWidget(separator());

        auto* node_title = new QLabel("<b>Selected Node</b>", this);
        layout->addWidget(node_title);

        node_detail_ = new QLabel("Click a node", this);
        node_detail_->setWordWrap(true);
        node_detail_->setMinimumHeight(50);
        layout->addWidget(node_detail_);

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
        renderer_->render(*graph_, coords_, communities_, node_size, 1.0);
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
    }

    using Graph = graph::Graph<graph::Node, graph::Edge>;

    rendering::GraphRenderer* renderer_{nullptr};
    rendering::GraphView* view_{nullptr};
    QBrush bg_brush_{Qt::black};
    TitleBar* title_bar_{nullptr};

    QLabel* file_label_{nullptr};
    QPushButton* render_btn_{nullptr};
    QComboBox* community_combo_{nullptr};
    QComboBox* layout_combo_{nullptr};
    QSlider* iterations_slider_{nullptr};
    QSlider* node_size_slider_{nullptr};
    QCheckBox* community_region_check_{nullptr};

    QComboBox* community_select_combo_{nullptr};
    QLabel* community_detail_{nullptr};
    QLabel* node_detail_{nullptr};
    QLabel* log_label_{nullptr};

    QLabel* stats_nodes_{nullptr};
    QLabel* stats_edges_{nullptr};
    QLabel* stats_communities_{nullptr};
    QLabel* stats_modularity_{nullptr};

    std::unique_ptr<Graph> graph_;
    community::CommunityResult communities_;
    layout::CoordinateMap coords_;
    std::string layout_name_{"f"};
    std::string community_method_{"louvain"};

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
};

}  // namespace satgraf::gui

Q_DECLARE_METATYPE(satgraf::community::CommunityResult)
Q_DECLARE_METATYPE(satgraf::layout::CoordinateMap)
