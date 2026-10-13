set(CMAKE_AUTOMOC ON)

add_executable(slicem_gui WIN32 main.cpp)
target_link_libraries(slicem_gui PRIVATE slicem_engine Qt6::Widgets slicem_warnings)
set_target_properties(slicem_gui PROPERTIES OUTPUT_NAME slicem-gui)
