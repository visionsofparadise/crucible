include(FetchContent)

# Phase 3 resolved the Windows-Debug CRT problem at the top-level
# CMakeLists by forcing the release CRT (/MD) in every config. With that
# in place, the Catch2 suite (including all Faust-linking tests) builds
# cleanly under windows-debug as well as windows-release, so no gate here.

# Pin Catch2 to v3.14.0 (matches gain-shaper).
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.14.0
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

add_executable(crucible_tests
  test_main.cpp
  test_faust_smoke.cpp
  test_stdlib_extractor.cpp
  test_faust_runtime.cpp
  test_source_loader.cpp
  test_schema.cpp
  test_slot_pool.cpp
  test_param_bridge.cpp
  test_meters_bridge.cpp
  test_midi_bridge.cpp)

target_include_directories(crucible_tests PRIVATE ../Core)

target_link_libraries(crucible_tests PRIVATE
  Catch2::Catch2
  CrucibleCore
  faust::withllvm
  juce::juce_dsp
  juce::juce_audio_basics
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

target_compile_features(crucible_tests PRIVATE cxx_std_17)

# JUCE module compile defs are required when linking against any JUCE
# module from a non-juce_add_plugin target. Disable web-browser/curl/etc
# pulls we don't need.
target_compile_definitions(crucible_tests PRIVATE
  JUCE_STANDALONE_APPLICATION=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  # Enables MessageManager::runDispatchLoopUntil for the message-loop
  # integration test (test_source_loader). Test-target only; no plugin
  # or product code is affected. runDispatchLoop() alone returns early
  # on macOS's headless run loop, so the SourceLoader watcher test pumps
  # in bounded slices instead.
  JUCE_MODAL_LOOPS_PERMITTED=1)

include(CTest)
include(Catch)
catch_discover_tests(crucible_tests)
