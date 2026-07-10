#pragma once

// Crucible Core — umbrella header.
//
// The shared core library for the Crucible FX and Crucible Instrument
// plugin variants. Phase 2 introduces the APVTS slot-pool layout helper
// (`SlotPool.h`); subsequent phases will land libfaust integration, JSON
// UI extraction, SVG block-diagram rendering, webview plumbing, file-watch
// hot reload, and parameter routing as additional headers re-exported here.

#include "SlotPool.h"
#include "StdlibExtractor.h"
#include "FaustRuntime.h"
#include "SourceLoader.h"
#include "Schema.h"
#include "ParamBridge.h"
#include "MetersBridge.h"
#include "MidiBridge.h"

// `WebviewBridge.h` is intentionally NOT re-exported here: it depends
// on JUCE's webview headers (gated behind `JUCE_WEB_BROWSER=1`), which
// Core itself is compiled without. The plugin variants (FX,
// Instrument) define `JUCE_WEB_BROWSER=1` and include
// `Core/WebviewBridge.h` directly from their editor source files.

namespace crucible {}
