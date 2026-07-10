#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "FaustRuntime.h"
#include "MetersBridge.h"
#include "ParamBridge.h"
#include "Schema.h"
#include "SourceLoader.h"

namespace crucible {

// WebviewBridge — owns the `juce::WebBrowserComponent`, the message
// channel between webview and Core, the Windows click-on-top
// behaviour, external-link interception, and the resource provider
// that serves the bundled UI (release) or the Vite dev server URL
// (debug).
//
// The bridge is a `juce::Component`. The PluginEditor adds it as a
// child and sizes it to fill the editor (920×760).
//
// Threading: all public methods are MESSAGE THREAD. The webview's
// native-function callbacks, resource-provider lambda, and the 30 Hz
// drain timer all run on the message thread.
class WebviewBridge : public juce::Component
{
public:
    enum class Variant { FX, Instrument };

    // Provider for the most-recent CompileResult cached by the
    // processor (lives across editor close/reopen). Returns nullptr if
    // no compile has ever landed. Used to replay state into a freshly-
    // mounted webview so the editor pane shows the last source +
    // schema instead of starting blank.
    using CompileResultProvider = std::function<const CompileResult*()>;

    WebviewBridge (juce::AudioProcessorValueTreeState& apvts,
                   FaustRuntime&                        runtime,
                   ParamBridge&                         paramBridge,
                   SourceLoader&                        sourceLoader,
                   MetersBridge&                        meters,
                   Variant                              variant,
                   CompileResultProvider                getCachedCompileResult);
    ~WebviewBridge() override;

    // juce::Component
    void resized() override;

    // Hooks invoked by the PluginProcessor's compile/source/error
    // callbacks (which already hop to the message thread). All emit
    // one `evaluateJavascript` call into the webview.
    //
    // `onCompileResult` receives the raw runtime CompileResult plus a
    // pointer to the schema *already* annotated by ParamBridge
    // (rebindForSchema is called by the processor before invoking the
    // bridge). `schema` may be null on failure or when the result is
    // not successful.
    void onCompileResult (const CompileResult&  result,
                          const ui::Schema*     schemaOrNull);

    void onSourceChanged (const juce::String& source,
                          const juce::File&   path);

    void onInputError    (const SourceLoader::InputError& error);

private:
    // 30 Hz UI-thread pump that drains the MetersBridge and emits one
    // `window.crucible.onMeterFrame(...)` per tick. Also drains the
    // runtime's retire-queue so old DSP instances are freed on the
    // message thread.
    class DrainPump : public juce::Timer
    {
    public:
        DrainPump (WebviewBridge& owner);
        void timerCallback() override;
    private:
        WebviewBridge& owner;
    };

    // Helper: emit `window.crucible.<channel>(<json>)` into the
    // webview. No-op if the webview is not yet ready.
    void emit (const juce::String& channel, const juce::String& jsonPayload);

    juce::AudioProcessorValueTreeState& apvts;
    FaustRuntime&                       runtime;
    ParamBridge&                        paramBridge;
    SourceLoader&                       sourceLoader;
    MetersBridge&                       meters;
    Variant                             variant;

    // Per-slot parameter relays. One `WebSliderRelay` per float slot
    // (`slot.f.000`–`slot.f.127`), one `WebToggleButtonRelay` per bool
    // slot (`slot.b.000`–`slot.b.031`). Bound to the APVTS parameter
    // of the same id via `WebSliderParameterAttachment` /
    // `WebToggleButtonParameterAttachment`. The webview reads/writes
    // through these per the JS bridge in `lib/crucible-bridge.ts`.
    std::vector<std::unique_ptr<juce::WebSliderRelay>>                    floatRelays;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>      floatAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>>              boolRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>>boolAttachments;

    // Holds the latest svgRoot directory so the resource provider can
    // serve `/svg/<filename>` from the right place. Atomic so the
    // resource-provider lambda (which may run off the message thread
    // on Windows WebView2) sees a published store immediately.
    juce::File              svgRootDir;

    CompileResultProvider   getCachedCompileResult;

    std::unique_ptr<juce::WebBrowserComponent> webView;
    std::unique_ptr<DrainPump>                 drainPump;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebviewBridge)
};

} // namespace crucible
