set(CMAKE_AUTOMOC ON)

add_executable(slicem_gui WIN32 main.cpp main_window.cpp viewport.cpp updater.cpp)
target_link_libraries(slicem_gui PRIVATE slicem_engine Qt6::Widgets Qt6::OpenGLWidgets Qt6::Network slicem_warnings)
set_target_properties(slicem_gui PROPERTIES OUTPUT_NAME slicem-gui)

# Qt and the MinGW runtime live only on the MSYS2 UCRT64 PATH. Launched from Explorer, or from any
# shell that has not prepended C:\msys64\ucrt64\bin, the GUI dies with 0xC0000135 (DLL not found)
# before main() runs - no window, no message, nothing to debug. Deploying the runtime beside the
# binary is what makes build/gui/slicem-gui.exe something you can simply double-click.
option(SLICEM_DEPLOY_QT "Copy the Qt and MinGW runtime next to the built GUI" ON)

if(WIN32 AND SLICEM_DEPLOY_QT)
  get_target_property(_slicem_qmake Qt6::qmake IMPORTED_LOCATION)
  if(_slicem_qmake)
    get_filename_component(_slicem_qt_bin "${_slicem_qmake}" DIRECTORY)
  endif()

  find_program(SLICEM_POWERSHELL NAMES powershell pwsh)

  if(SLICEM_POWERSHELL AND _slicem_qt_bin)
    add_custom_command(TARGET slicem_gui POST_BUILD
      COMMAND "${SLICEM_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
              -File "${CMAKE_SOURCE_DIR}/scripts/deploy_runtime.ps1"
              -Exe "$<TARGET_FILE:slicem_gui>" -QtBin "${_slicem_qt_bin}"
      COMMENT "Deploying the Qt and MinGW runtime next to slicem-gui.exe"
      VERBATIM)
  else()
    message(WARNING
      "Runtime deployment is unavailable, so build/gui/slicem-gui.exe will only start from a "
      "shell that has the Qt bin directory on its PATH.")
  endif()
endif()
