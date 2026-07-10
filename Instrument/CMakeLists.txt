# Crucible Instrument — VST3 instrument variant. MIDI in -> audio out.
#
# NEEDS_WEBVIEW2 is the JUCE-side opt-in for the WebView2 backend on
# Windows; JUCE silently ignores it on other platforms but guarding makes
# the macOS / Linux configure path obviously safe (it does not depend on
# the Microsoft.Web.WebView2 NuGet package being present).
if(WIN32)
  juce_add_plugin(CrucibleInstrument
    COMPANY_NAME "ZCROSS"
    COMPANY_WEBSITE "https://zcross.media"
    PLUGIN_MANUFACTURER_CODE "ZCRS"
    PLUGIN_CODE "CrIn"
    FORMATS VST3 Standalone
    PRODUCT_NAME "Crucible Instrument"
    BUNDLE_ID "media.zcross.crucible.instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
    NEEDS_WEBVIEW2 TRUE
  )
else()
  juce_add_plugin(CrucibleInstrument
    COMPANY_NAME "ZCROSS"
    COMPANY_WEBSITE "https://zcross.media"
    PLUGIN_MANUFACTURER_CODE "ZCRS"
    PLUGIN_CODE "CrIn"
    FORMATS VST3 Standalone
    PRODUCT_NAME "Crucible Instrument"
    BUNDLE_ID "media.zcross.crucible.instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
    NEEDS_WEB_BROWSER TRUE
  )
endif()

target_sources(CrucibleInstrument PRIVATE
  PluginProcessor.cpp
  PluginEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../Core/WebviewBridge.cpp
)

target_compile_definitions(CrucibleInstrument PRIVATE
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

# JUCE_USE_WIN_WEBVIEW2 enables the WebView2 backend in juce_gui_extra; it
# is meaningless on macOS / Linux. Explicitly Windows-only for cross-
# platform clarity (matches the NEEDS_WEBVIEW2 guard above).
if(WIN32)
  target_compile_definitions(CrucibleInstrument PRIVATE
    JUCE_USE_WIN_WEBVIEW2=1
  )
endif()

target_link_libraries(CrucibleInstrument PRIVATE
  CrucibleCore
  CrucibleSkillData
  juce::juce_audio_utils
  juce::juce_audio_plugin_client
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

# UI bundling — release builds embed the Vite output via the shared
# `CrucibleUIData` target defined in Core/CMakeLists.txt, debug builds
# expect the dev server at `localhost:5173`. Selected by the top-level
# `CRUCIBLE_UI_DEV_SERVER` option. The `CRUCIBLE_UI_BUNDLED` macro is
# read by WebviewBridge.cpp (compiled into each plugin) to switch
# between the resource-provider path and the dev-server URL.
if(NOT CRUCIBLE_UI_DEV_SERVER)
  target_link_libraries(CrucibleInstrument PRIVATE CrucibleUIData)
  target_compile_definitions(CrucibleInstrument PRIVATE CRUCIBLE_UI_BUNDLED=1)
else()
  target_compile_definitions(CrucibleInstrument PRIVATE CRUCIBLE_UI_BUNDLED=0)
endif()
