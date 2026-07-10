# Crucible Core — shared library for the FX and Instrument plugin variants.
#
# Hosts the future libfaust integration (LLVM JIT compilation of Faust source
# at runtime), JSON UI extraction, SVG block diagram rendering, webview UI
# plumbing, file-watch hot reload, and parameter routing. The plugin targets
# in FX/ and Instrument/ link against this core and add only their format-
# specific bus configuration and category metadata.
#
# Phase 2 promotes Core to a STATIC library: the APVTS slot-pool layout
# helper is the first compiled translation unit. Subsequent phases will add
# FaustRuntime, ParamBridge, MetersBridge, etc. as additional sources here.

add_library(CrucibleCore STATIC)

target_sources(CrucibleCore PRIVATE
  SlotPool.cpp
  StdlibExtractor.cpp
  FaustRuntime.cpp
  SourceLoader.cpp
  Schema.cpp
  ParamBridge.cpp
  MetersBridge.cpp
  MidiBridge.cpp
)

# WebviewBridge.cpp is NOT compiled into Core because it uses
# `juce::WebBrowserComponent`, which requires `JUCE_WEB_BROWSER=1` in
# the JUCE module compile. Each plugin target compiles its own copy of
# WebviewBridge.cpp (added via `target_sources` in FX/Instrument
# CMakeLists) so the JUCE WebView symbols come from a single set of
# TUs — the plugin's own — and no duplicate-symbol clash with Core's
# JUCE module TUs occurs. The header is part of Core so the plugin
# editors can `#include <Core/WebviewBridge.h>`.

target_include_directories(CrucibleCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Phase 3.1: mirror Faust's stdlib (`*.lib` files) into the build tree
# with a `.faustlib` extension so CMake / juce_add_binary_data don't
# interpret them as static-library link inputs on Windows. The
# StdlibExtractor restores the `.lib` extension at runtime when
# materialising the files into the user data directory.
set(FAUST_STDLIB_BUNDLE_DIR ${CMAKE_CURRENT_BINARY_DIR}/faust-stdlib-bundle)
file(MAKE_DIRECTORY ${FAUST_STDLIB_BUNDLE_DIR})
file(GLOB FAUST_STDLIB_SRC ${FAUST_STDLIB_DIR}/*.lib)
if(NOT FAUST_STDLIB_SRC)
  message(FATAL_ERROR
    "Crucible: no Faust stdlib .lib files found under ${FAUST_STDLIB_DIR}. "
    "FetchFaust.cmake should have populated this directory from the Faust "
    "source tarball.")
endif()
set(FAUST_STDLIB_BUNDLED_FILES "")
foreach(_lib ${FAUST_STDLIB_SRC})
  get_filename_component(_name ${_lib} NAME_WE)
  set(_dst ${FAUST_STDLIB_BUNDLE_DIR}/${_name}.faustlib)
  configure_file(${_lib} ${_dst} COPYONLY)
  list(APPEND FAUST_STDLIB_BUNDLED_FILES ${_dst})
endforeach()

juce_add_binary_data(CrucibleStdlibData
  HEADER_NAME CrucibleStdlibData.h
  NAMESPACE   CrucibleStdlibData
  SOURCES     ${FAUST_STDLIB_BUNDLED_FILES}
)

# COPY CONTEXT skill document — bundled into a separate binary-data
# target so the stdlib target stays pure (`StdlibExtractor` iterates
# the stdlib namespace and would otherwise pick up the markdown file
# as a phantom `.lib`).
juce_add_binary_data(CrucibleSkillData
  HEADER_NAME CrucibleSkillData.h
  NAMESPACE   CrucibleSkillData
  SOURCES     ${CMAKE_CURRENT_SOURCE_DIR}/copy-context-skill.md
)

target_link_libraries(CrucibleCore PUBLIC
  CrucibleStdlibData
  faust::withllvm
  juce::juce_audio_utils
  juce::juce_audio_basics
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)

# The prebuilt libfaustwithllvm bundles LLVM, whose objects reference system
# compression / xml / terminfo libraries. The Windows prebuilt resolves these
# internally; on macOS/Linux they must be linked into the final binary. zlib is
# required (LLVM's Compression/CRC always reference it); the rest are linked
# only when present, since whether a given LLVM build uses them varies.
if(NOT WIN32)
  find_package(ZLIB REQUIRED)
  target_link_libraries(CrucibleCore PUBLIC ZLIB::ZLIB)
  foreach(_faust_syslib zstd xml2 ncurses tinfo)
    find_library(FAUST_SYSLIB_${_faust_syslib} NAMES ${_faust_syslib})
    if(FAUST_SYSLIB_${_faust_syslib})
      target_link_libraries(CrucibleCore PUBLIC ${FAUST_SYSLIB_${_faust_syslib}})
    endif()
  endforeach()
endif()

# The Faust version is consumed by StdlibExtractor for the versioned
# extraction folder name. Source it from FetchFaust.cmake's variable so
# the two stay in sync.
target_compile_definitions(CrucibleCore PUBLIC
  CRUCIBLE_FAUST_VERSION="${FAUST_VERSION}"
)

# JUCE module compile defs are required when linking against any JUCE
# module from a non-juce_add_plugin target. PRIVATE so they apply only when
# compiling Core's own TUs (including JUCE module .cpp files pulled in via
# `juce::juce_*` INTERFACE_SOURCES); the plugin variants compile their own
# copies of the JUCE modules with their own definitions (e.g.
# JUCE_WEB_BROWSER=1, JUCE_USE_WIN_WEBVIEW2=1) and Core's static-library
# symbols are pulled in lazily by the linker only where the plugin lacks
# them, so the per-target define mismatch is harmless.
target_compile_definitions(CrucibleCore PRIVATE
  JUCE_STANDALONE_APPLICATION=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
)

# Shared UI binary-data target — both FX and Instrument link against this so
# the Vite bundle in `ui/dist/` is embedded once. Defined here (rather than
# at the top level) to keep the dependency on `CrucibleUI` colocated with
# the rest of the JUCE wiring.
#
# `juce_add_binary_data` evaluates its SOURCES list at configure time, so
# `ui/dist/` must already exist by then. The custom command in the top-level
# CMakeLists is a build-time recipe — fine for incremental rebuilds, but
# useless on a fresh tree. Match gain-shaper's CI convention: run
# `cd ui && npm ci && npm run build` before `cmake --preset`, or pass
# `-DCRUCIBLE_UI_DEV_SERVER=ON` for a debug-mode build that talks to the
# Vite dev server instead.
if(NOT CRUCIBLE_UI_DEV_SERVER)
  if(NOT EXISTS ${UI_DIST_DIR}/index.html)
    message(FATAL_ERROR
      "Crucible: ${UI_DIST_DIR}/index.html not found. Build the UI bundle "
      "before configuring (cd ui && npm ci && npm run build), or pass "
      "-DCRUCIBLE_UI_DEV_SERVER=ON to skip bundling and use the dev server.")
  endif()

  file(GLOB_RECURSE CRUCIBLE_UI_BUNDLE_FILES CONFIGURE_DEPENDS ${UI_DIST_DIR}/*)

  juce_add_binary_data(CrucibleUIData
    HEADER_NAME CrucibleUIData.h
    NAMESPACE   CrucibleUIData
    SOURCES     ${CRUCIBLE_UI_BUNDLE_FILES}
  )

  add_dependencies(CrucibleUIData CrucibleUI)
endif()
