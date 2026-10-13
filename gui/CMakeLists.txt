set(CMAKE_AUTOMOC ON)

add_executable(slicem_gui WIN32 main.cpp main_window.cpp viewport.cpp)
target_link_libraries(slicem_gui PRIVATE slicem_engine Qt6::Widgets Qt6::OpenGLWidgets slicem_warnings)
set_target_properties(slicem_gui PROPERTIES OUTPUT_NAME slicem-gui)
