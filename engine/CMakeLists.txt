add_library(slicem_engine STATIC
  geometry/boolean.cpp
  geometry/offset.cpp
  geometry/polygon.cpp
  geometry/segment_ops.cpp
  slicing/mesh.cpp
  slicing/stl_reader.cpp
  slicing/slicer.cpp
  slicing/perimeters.cpp
  slicing/infill.cpp
  slicing/surfaces.cpp
  gcode/gcode_writer.cpp
  gcode/slice_to_gcode.cpp
)

target_include_directories(slicem_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(slicem_engine PRIVATE slicem_warnings)
