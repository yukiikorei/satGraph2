add_library(satgraf_gui STATIC
    include/satgraf_gui/graph_renderer.hpp
    include/satgraf_gui/export.hpp
    include/satgraf_gui/main_window.hpp
    include/satgraf_gui/graph_view_3d.hpp
)

target_include_directories(satgraf_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(satgraf_gui PUBLIC satgraf_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::OpenGLWidgets)
