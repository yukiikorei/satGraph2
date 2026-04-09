add_executable(satgraf main.cpp)
target_link_libraries(satgraf PRIVATE satgraf_core satgraf_gui CLI11::CLI11)
