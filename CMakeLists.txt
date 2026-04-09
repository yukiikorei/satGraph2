cmake_minimum_required(VERSION 3.21)
project(satgraf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---
find_package(igraph REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets Qml Quick OpenGL OpenGLWidgets REQUIRED)
find_package(OpenCL REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Catch2 3 REQUIRED)

# --- Qt automation ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${CMAKE_CXX_COMPILER_LAUNCHER})

# --- Subdirectories ---
add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)

# --- CPack DEB packaging ---
install(TARGETS satgraf RUNTIME DESTINATION bin)
install(FILES icon.png DESTINATION share/icons/hicolor/1024x1024/apps RENAME satgraf.png)
install(FILES icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME satgraf.svg)
install(FILES satgraf.desktop DESTINATION share/applications)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "satgraf")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SAT solver visualization tool")
set(CPACK_PACKAGE_DESCRIPTION "Interactive graph-based visualization for SAT solver
community structure and variable evolution. Supports multiple layout algorithms
(Fruchterman-Reingold, ForceAtlas2, Grid), community detection (Louvain, Label
Propagation), and real-time solver event tracking via external solver integration.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ekuiter/SATGraf")
set(CPACK_PACKAGE_CONTACT "Jinghu Liang")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jinghu Liang")
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libigraph3t64 (>= 0.10), libqt6core6t64 (>= 6.4), libqt6gui6t64 (>= 6.4), libqt6widgets6t64 (>= 6.4), ocl-icd-libopencl1 (>= 2.3), libstdc++6 (>= 13)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "opencl-icd")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

include(CPack)
