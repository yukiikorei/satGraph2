#include <CLI/CLI.hpp>
#include <satgraf/dimacs_parser.hpp>
#include <satgraf/graph.hpp>
#include <satgraf/community_detector.hpp>
#include <satgraf/layout.hpp>

#include <satgraf_gui/main_window.hpp>
#include <satgraf_gui/export.hpp>

#include <QApplication>
#include <QImage>
#include <QIcon>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    CLI::App app{"satGraf — SAT solver visualization tool", "satgraf"};

    std::string input_file;
    app.add_option("-i,--input", input_file, "Input DIMACS CNF file")
        ->check(CLI::ExistingFile);

    std::string solver_path;
    app.add_option("-s,--solver", solver_path, "Path to SAT solver binary (required for evolution mode)");

    std::string layout_name{"f"};
    app.add_option("-l,--layout", layout_name, "Layout algorithm (f, fgpu, forceAtlas2, kk, c, grid, gkk)")
        ->capture_default_str();

    std::string mode{"com"};
    app.add_option("-m,--mode", mode, "Visualization mode: com (community), imp (implication), evo (evolution), exp (export)")
        ->capture_default_str()
        ->check([](const std::string& val) -> std::string {
            if (val != "com" && val != "imp" && val != "evo" && val != "exp") {
                return "Mode must be one of: com, imp, evo, exp";
            }
            return {};
        });

    std::string output_file;
    app.add_option("-o,--output", output_file, "Output file for export mode (PNG/JPEG)");

    bool headless = false;
    app.add_flag("--headless", headless, "Run without GUI (for export/batch processing)");

    std::string community_method{"louvain"};
    app.add_option("--community", community_method, "Community detection method (louvain, cnm)")
        ->capture_default_str();

    int iterations = 500;
    app.add_option("--iterations", iterations, "Number of layout iterations")
        ->capture_default_str()
        ->check(CLI::PositiveNumber);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (mode == "evo" && solver_path.empty()) {
        std::cerr << "Error: Evolution mode requires a solver binary. Use -s <path>.\n";
        return 1;
    }

    if (!solver_path.empty()) {
        fs::path sp(solver_path);
        if (!fs::exists(sp)) {
            std::cerr << "Error: Solver binary not found: " << solver_path << "\n";
            return 1;
        }
    }

    if (mode == "exp" && output_file.empty()) {
        std::cerr << "Error: Export mode requires an output file. Use -o <path>.\n";
        return 1;
    }

    if ((headless || mode == "exp") && input_file.empty()) {
        std::cerr << "Error: Input CNF file required in headless mode. Use -i <path>.\n";
        return 1;
    }

    if (headless || mode == "exp") {
        std::cout << "satgraf v0.1.0 (headless)\n";

        try {
            satgraf::dimacs::Parser parser;
            auto graph = parser.parse(input_file, satgraf::dimacs::Mode::VIG);
            std::cout << "Parsed " << graph.nodeCount() << " variables, "
                      << graph.edgeCount() << " edges\n";

            auto detector = satgraf::community::DetectorFactory::instance().create(community_method);
            auto communities = detector->detect(graph);

            auto layout = satgraf::layout::LayoutFactory::instance().create(layout_name);
            auto coords = layout->compute(graph, [](double p) {
                if (p >= 1.0) std::cout << "Layout complete\n";
            });

            if (!output_file.empty()) {
                QImage img = satgraf::export_::render_headless(
                    graph, coords, communities, 1024, 1024, 10.0,
                    [](double p) {
                        if (p >= 1.0) std::cout << "Render complete\n";
                    });
                if (img.save(QString::fromStdString(output_file))) {
                    std::cout << "Exported: " << output_file << "\n";
                } else {
                    std::cerr << "Error: Failed to export image\n";
                    return 1;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    QApplication qt_app(argc, argv);
    qt_app.setWindowIcon(QIcon(":/satgraf/icon.png"));
    qRegisterMetaType<satgraf::community::CommunityResult>("satgraf::community::CommunityResult");
    qRegisterMetaType<satgraf::layout::CoordinateMap>("satgraf::layout::CoordinateMap");
    satgraf::gui::MainWindow window;

    if (!input_file.empty()) {
        QTimer::singleShot(0, &window, [&window, &input_file]() {
            window.open_file_path(QString::fromStdString(input_file));
        });
    }

    window.show();
    return qt_app.exec();
}
