add_executable(satgraf_tests test_graph_model.cpp test_dimacs_parser.cpp test_community_detection.cpp test_layout.cpp test_solver.cpp test_evolution.cpp)
target_link_libraries(satgraf_tests PRIVATE satgraf_core Catch2::Catch2WithMain)

add_executable(satgraf_gui_tests test_gui.cpp)
target_link_libraries(satgraf_gui_tests PRIVATE satgraf_core satgraf_gui Catch2::Catch2WithMain Qt6::Widgets)
target_compile_definitions(satgraf_gui_tests PRIVATE QT_DISABLE_DEPRECATED_BEFORE=0x060000)

include(CTest)
include(Catch)
catch_discover_tests(satgraf_tests)
catch_discover_tests(satgraf_gui_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
