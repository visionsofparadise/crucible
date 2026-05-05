cmake_minimum_required(VERSION 3.22)

project(Crucible VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
  add_custom_command(
    OUTPUT ${UI_DIST_DIR}/index.html
    COMMAND ${NPM_EXE} install
    COMMAND ${NPM_EXE} run build
    WORKING_DIRECTORY ${UI_SOURCE_DIR}
    DEPENDS ${UI_SOURCE_DIR}/package.json
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
