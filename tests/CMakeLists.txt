include(FetchContent)

# Pin Catch2 to v3.14.0 (matches gain-shaper).
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.14.0
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(CrucibleTests
  test_placeholder.cpp)

target_include_directories(CrucibleTests PRIVATE ../Core)

target_link_libraries(CrucibleTests PRIVATE
  Catch2::Catch2WithMain
  juce::juce_dsp
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

# JUCE module compile defs are required when linking against any JUCE
# module from a non-juce_add_plugin target. Disable web-browser/curl/etc
# pulls we don't need.
target_compile_definitions(CrucibleTests PRIVATE
  JUCE_STANDALONE_APPLICATION=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0)

include(CTest)
include(Catch)
catch_discover_tests(CrucibleTests)
