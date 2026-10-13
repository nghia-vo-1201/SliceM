add_library(slicem_engine STATIC
  geometry/polygon.cpp
  slicing/mesh.cpp
  slicing/stl_reader.cpp
  slicing/slicer.cpp
)

target_include_directories(slicem_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(slicem_engine PRIVATE slicem_warnings)
