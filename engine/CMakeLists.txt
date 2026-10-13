add_library(slicem_engine STATIC
  geometry/offset.cpp
  geometry/polygon.cpp
  slicing/mesh.cpp
  slicing/stl_reader.cpp
  slicing/slicer.cpp
  slicing/perimeters.cpp
  slicing/infill.cpp
)

target_include_directories(slicem_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(slicem_engine PRIVATE slicem_warnings)
