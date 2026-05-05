# Crucible Core — shared library for the FX and Instrument plugin variants.
#
# Hosts the future libfaust integration (LLVM JIT compilation of Faust source
# at runtime), JSON UI extraction, SVG block diagram rendering, webview UI
# plumbing, file-watch hot reload, and parameter routing. The plugin targets
# in FX/ and Instrument/ link against this core and add only their format-
# specific bus configuration and category metadata.
#
# At scaffold time the only source artefact is the placeholder header
# `CrucibleCore.h`; a CMake static library with no compiled units must be
# declared as INTERFACE so it carries include paths and link dependencies
# without requiring a translation unit.

add_library(CrucibleCore INTERFACE)

target_include_directories(CrucibleCore INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(CrucibleCore INTERFACE
  juce::juce_audio_utils
  juce::juce_audio_basics
  juce::juce_dsp
  juce::juce_gui_extra
)

# Shared UI binary-data target — both FX and Instrument link against this so
# the Vite bundle in `ui/dist/` is embedded once. Defined here (rather than
# at the top level) to keep the dependency on `CrucibleUI` colocated with
# the rest of the JUCE wiring.
if(NOT CRUCIBLE_UI_DEV_SERVER)
  file(GLOB_RECURSE CRUCIBLE_UI_BUNDLE_FILES CONFIGURE_DEPENDS ${UI_DIST_DIR}/*)

  juce_add_binary_data(CrucibleUIData
    HEADER_NAME CrucibleUIData.h
    NAMESPACE   CrucibleUIData
    SOURCES     ${CRUCIBLE_UI_BUNDLE_FILES}
  )

  add_dependencies(CrucibleUIData CrucibleUI)
endif()
