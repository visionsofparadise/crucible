cmake_minimum_required(VERSION 3.22)

project(Crucible VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CrucibleCore (and the JUCE module TUs compiled into it) is a STATIC library
# linked into the shared VST3 .so. On Linux that requires position-independent
# code; without it ld rejects R_X86_64_TPOFF32 relocations. No-op on Windows,
# default on macOS.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Phase 3 CRT decision (option 2): Faust 2.85.5's Windows prebuilt
# libfaustwithllvm.lib is built with the release CRT (/MD,
# _ITERATOR_DEBUG_LEVEL=0). To allow Debug builds of Crucible to link
# Faust without LNK2038 across thousands of LLVM TUs, force every config
# (including Debug) to link the release CRT (/MD). Trade-off accepted:
# Debug builds lose iterator debugging; they keep symbols, assertions,
# and JUCE's debug behaviour. Recorded in design-vst.md /
# plan-bootstrap-vst.md Phase 3.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:>DLL")

  # Strip MSVC's default `/D_DEBUG` and `/RTC1` from the Debug config.
  # When the release CRT is selected, the C++ standard library headers
  # expect `_DEBUG` to be undefined; otherwise compilation emits calls
  # to `_malloc_dbg` / `_free_dbg` / `_CrtDbgReport` which live only in
  # the debug CRT and produce LNK2019s at link time. `/RTC1` likewise
  # needs the debug heap. Iterator-debug etc. come back the day Faust
  # provides a debug-CRT prebuilt; for now keep the release CRT.
  string(REPLACE "/D_DEBUG"  "" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
  string(REPLACE "/D_DEBUG"  "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/RTC1"     "" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
  string(REPLACE "/RTC1"     "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

  # JUCE's juce_recommended_config_flags adds `_DEBUG=1` for Debug
  # builds via its own DEFINES list, independently of MSVC's flag.
  # That still ends up triggering the debug-CRT include path inside
  # <yvals_core.h>. Negate it explicitly in Debug too, so the headers
  # consistently pick the release CRT path.
  add_compile_options ("$<$<CONFIG:Debug>:/U_DEBUG>")
endif()

option(CRUCIBLE_UI_DEV_SERVER "Use Vite dev server at localhost:5173 instead of bundled assets" OFF)
option(CRUCIBLE_BUILD_TESTS   "Build Catch2 tests" ON)

include(FetchContent)

# Pin JUCE to 8.0.12 exactly. Do not bump without a Design decision.
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.12
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Faust 2.85.5 prebuilt binaries + source tarball — see CMake/FetchFaust.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/FetchFaust.cmake)

# UI bundle orchestration — release builds run `npm install && npm run build`
# in `ui/` to produce `ui/dist/` before `juce_add_binary_data` consumes it.
# Debug builds (`CRUCIBLE_UI_DEV_SERVER=ON`) skip this and rely on the Vite
# dev server at `localhost:5173` instead. The shared bundle is consumed by
# both Crucible FX and Crucible Instrument via a single binary-data target
# defined in Core/.
set(UI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(UI_DIST_DIR  ${UI_SOURCE_DIR}/dist)

if(NOT CRUCIBLE_UI_DEV_SERVER)
  find_program(NPM_EXE npm REQUIRED)
  # CONFIGURE_DEPENDS makes the glob re-evaluated on every build so new
  # source files added to ui/src are picked up without a reconfigure.
  # Each glob entry becomes a dependency of the custom command — editing
  # any .ts/.tsx/.css invalidates ui/dist/index.html and re-runs the
  # Vite build before juce_add_binary_data re-embeds the dist.
  # GLOB_RECURSE is restricted to src/ so it can't match `ui/dist/index.html`
  # (the custom-command OUTPUT), which would create a self-cycle. Top-level
  # config files are listed explicitly below.
  file(GLOB_RECURSE CRUCIBLE_UI_SRC_SOURCES CONFIGURE_DEPENDS
       ${UI_SOURCE_DIR}/src/*.ts
       ${UI_SOURCE_DIR}/src/*.tsx
       ${UI_SOURCE_DIR}/src/*.css
       ${UI_SOURCE_DIR}/src/*.svg)
  set(CRUCIBLE_UI_SOURCES
       ${CRUCIBLE_UI_SRC_SOURCES}
       ${UI_SOURCE_DIR}/index.html
       ${UI_SOURCE_DIR}/vite.config.ts
       ${UI_SOURCE_DIR}/tsconfig.json)
  add_custom_command(
    OUTPUT ${UI_DIST_DIR}/index.html
    COMMAND ${NPM_EXE} install
    COMMAND ${NPM_EXE} run build
    WORKING_DIRECTORY ${UI_SOURCE_DIR}
    DEPENDS ${UI_SOURCE_DIR}/package.json ${CRUCIBLE_UI_SOURCES}
    COMMENT "Building Crucible UI bundle"
  )
  add_custom_target(CrucibleUI DEPENDS ${UI_DIST_DIR}/index.html)
endif()

add_subdirectory(Core)
add_subdirectory(FX)
add_subdirectory(Instrument)

if(CRUCIBLE_BUILD_TESTS)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
