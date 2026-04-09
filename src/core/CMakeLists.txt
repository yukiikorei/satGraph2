add_library(satgraf_core STATIC
    include/satgraf/types.hpp
    include/satgraf/node.hpp
    include/satgraf/edge.hpp
    include/satgraf/clause.hpp
    include/satgraf/graph.hpp
    include/satgraf/community_node.hpp
    include/satgraf/community_graph.hpp
    include/satgraf/csr.hpp
    include/satgraf/union_find.hpp
    include/satgraf/dimacs_writer.hpp
    include/satgraf/dimacs_parser.hpp
    include/satgraf/community_detector.hpp
    include/satgraf/layout.hpp
    include/satgraf/solver.hpp
    include/satgraf/evolution.hpp
)

target_include_directories(satgraf_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_path(IGRAPH_VERSION_DIR
    NAMES igraph_version.h
    PATHS /usr/include/${CMAKE_LIBRARY_ARCHITECTURE}/igraph
          /usr/local/include/${CMAKE_LIBRARY_ARCHITECTURE}/igraph
)
if(IGRAPH_VERSION_DIR)
    target_include_directories(satgraf_core SYSTEM PUBLIC ${IGRAPH_VERSION_DIR})
endif()

target_link_libraries(satgraf_core PUBLIC igraph::igraph Eigen3::Eigen nlohmann_json::nlohmann_json OpenCL::OpenCL)
