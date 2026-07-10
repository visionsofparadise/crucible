#include "WebviewBridge.h"

#include <cstring>

#include <CrucibleSkillData.h>
#include "SlotPool.h"

#if CRUCIBLE_UI_BUNDLED
 #include <CrucibleUIData.h>
#endif

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace crucible {

namespace
{

// Root URL of the resource provider. The bundled UI is served from
// here; any navigation to a different origin is routed to the system
// browser. Matches gain-shaper's "{plugin}.local" pattern.
constexpr const char* kCrucibleOrigin = "https://crucible.local/";

// Map a lowercase ".ext" to a MIME type. Mirrors gain-shaper's table.
juce::String mimeTypeForExtension (const juce::String& extWithDot)
{
    const auto ext = extWithDot.toLowerCase();
    if (ext == ".html")  return "text/html";
    if (ext == ".js")    return "application/javascript";
    if (ext == ".mjs")   return "application/javascript";
    if (ext == ".css")   return "text/css";
    if (ext == ".svg")   return "image/svg+xml";
    if (ext == ".png")   return "image/png";
    if (ext == ".jpg")   return "image/jpeg";
    if (ext == ".jpeg")  return "image/jpeg";
    if (ext == ".json")  return "application/json";
    if (ext == ".woff")  return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

// Read the COPY CONTEXT skill markdown out of the
// `CrucibleSkillData` binary-data blob and return it as a heap string.
// The blob's filename mangles to `copycontextskill_md` per
// juce_add_binary_data's name-mangling rules; we look it up
// explicitly so a future rename of the source file triggers a build
// failure here rather than a silent empty clipboard.
juce::String getCopyContextDocument()
{
    int size = 0;
    const auto* data = CrucibleSkillData::getNamedResource ("copycontextskill_md", size);
    if (data == nullptr || size <= 0)
        return {};
    return juce::String::fromUTF8 (data, size);
}

// SVG resource lookup. The WebviewBridge holds a `currentSvgDir`
// `juce::File` whose pointer is updated on every successful compile.
// The resource-provider lambda reads it through this accessor so
// `/svg/<filename>` requests resolve against the latest SVG output
// folder; older folders are deleted by FaustRuntime when the next
// compile retires its predecessor.
std::atomic<const juce::File*> currentSvgDirPtr { nullptr };

std::optional<juce::WebBrowserComponent::Resource> resolveSvgFile (const juce::String& filename)
{
    const auto* dir = currentSvgDirPtr.load (std::memory_order_acquire);
    if (dir == nullptr || *dir == juce::File())
        return std::nullopt;

    // Recursively search the SVG output folder for the requested
    // filename. Faust emits a flat folder when the program has no
    // subdiagrams; nested folders when it does. Filename match is
    // case-sensitive; Faust always emits lowercase.
    juce::Array<juce::File> candidates;
    dir->findChildFiles (candidates, juce::File::findFiles, true, filename);
    if (candidates.isEmpty()) return std::nullopt;

    const auto file = candidates.getFirst();
    juce::MemoryBlock contents;
    if (! file.loadFileAsData (contents))
        return std::nullopt;

    juce::WebBrowserComponent::Resource resource;
    resource.data.resize (contents.getSize());
    std::memcpy (resource.data.data(), contents.getData(), contents.getSize());
    resource.mimeType = "image/svg+xml";
    return resource;
}

#if CRUCIBLE_UI_BUNDLED

// `juce_add_binary_data` mangles original filenames to C identifiers
// (dots/hyphens to underscores). Walk the mangled→original map to
// reverse the lookup. Pattern lifted from gain-shaper.
const char* findMangledName (const juce::String& originalFilename)
{
    for (int i = 0; i < CrucibleUIData::namedResourceListSize; ++i)
        if (originalFilename == CrucibleUIData::originalFilenames[i])
            return CrucibleUIData::namedResourceList[i];
    return nullptr;
}

std::optional<juce::WebBrowserComponent::Resource> bundledResourceProvider (const juce::String& url,
                                                                            const juce::String& variantSlug)
{
    auto path = url;
    while (path.startsWithChar ('/')) path = path.substring (1);
    if (path.isEmpty()) path = "index.html";

    // `/svg/<filename>` requests are resolved against the runtime's
    // current SVG output folder rather than the bundled UI assets.
    if (path.startsWith ("svg/"))
    {
        const auto svgName = path.substring (4);
        return resolveSvgFile (svgName);
    }

    const auto fileName = path.fromLastOccurrenceOf ("/", false, false);
    const auto* mangled = findMangledName (fileName);
    if (mangled == nullptr) return std::nullopt;

    int dataSize = 0;
    const auto* dataPtr = CrucibleUIData::getNamedResource (mangled, dataSize);
    if (dataPtr == nullptr || dataSize <= 0) return std::nullopt;

    // index.html — inject a `<script>` setting `window.__crucibleVariant`
    // before any other script runs, so the JS bundle never sees a null
    // variant on first render. Replaces the prior URL-query approach.
    if (fileName == "index.html")
    {
        const juce::String html (reinterpret_cast<const char*> (dataPtr),
                                 static_cast<size_t> (dataSize));
        const juce::String injection = "<script>window.__crucibleVariant=\"" + variantSlug + "\";</script>";
        const auto headEnd = html.indexOf ("</head>");
        const juce::String rewritten = (headEnd >= 0)
            ? html.substring (0, headEnd) + injection + html.substring (headEnd)
            : injection + html;

        juce::WebBrowserComponent::Resource resource;
        const auto utf8 = rewritten.toRawUTF8();
        const auto len  = static_cast<size_t> (rewritten.getNumBytesAsUTF8());
        resource.data.resize (len);
        std::memcpy (resource.data.data(), utf8, len);
        resource.mimeType = "text/html";
        return resource;
    }

    juce::WebBrowserComponent::Resource resource;
    resource.data.resize (static_cast<size_t> (dataSize));
    std::memcpy (resource.data.data(), dataPtr, static_cast<size_t> (dataSize));
    resource.mimeType = mimeTypeForExtension (fileName.fromLastOccurrenceOf (".", true, false));
    return resource;
}

#else // !CRUCIBLE_UI_BUNDLED

// Dev-server mode: only the `/svg/*` path is served by the resource
// provider (the rest comes from Vite's dev server). Pattern matches
// the bundled path so both modes resolve diagram requests
// identically.
std::optional<juce::WebBrowserComponent::Resource> devSvgResourceProvider (const juce::String& url)
{
    auto path = url;
    while (path.startsWithChar ('/')) path = path.substring (1);
    const auto queryStart = path.indexOfChar ('?');
    if (queryStart >= 0) path = path.substring (0, queryStart);
    if (path.startsWith ("svg/"))
        return resolveSvgFile (path.substring (4));
    return std::nullopt;
}

#endif // CRUCIBLE_UI_BUNDLED

// SinglePageBrowser — subclass that intercepts navigation. Any URL
// outside our resource-provider origin (release) or the Vite dev
// server (debug) is routed to the system browser via
// `juce::URL::launchInDefaultBrowser`. Returning false prevents the
// webview from navigating internally.
class SinglePageBrowser : public juce::WebBrowserComponent
{
public:
    using juce::WebBrowserComponent::WebBrowserComponent;

    bool pageAboutToLoad (const juce::String& newURL) override
    {
        // A same-origin navigation to a `.svg` is a Faust diagram
        // hyperlink that escaped the in-webview drill-in interceptor.
        // It resolves to no served resource and would replace the React
        // app with a failed-load error page. Cancel silently — the UI
        // de-links these anchors, so this only fires as a backstop.
        const auto providerRoot = juce::WebBrowserComponent::getResourceProviderRoot();
        if ((newURL.startsWith (providerRoot) || newURL.startsWith ("http://localhost:5173"))
            && newURL.endsWithIgnoreCase (".svg"))
        {
            return false;
        }

        // Allow the initial blank/about page that JUCE may use
        // internally, and any URL under our own origin / dev server.
        if (newURL.isEmpty()
            || newURL.startsWith ("about:")
            || newURL.startsWith ("data:")
            || newURL == providerRoot
            || newURL.startsWith (providerRoot)
            || newURL.startsWith ("http://localhost:5173"))
        {
            return true;
        }

        // External link: route to the system browser, do not navigate.
        juce::URL (newURL).launchInDefaultBrowser();
        return false;
    }
};

// JSON helpers built on juce::DynamicObject so payloads are safely
// quoted/escaped. Used by `onCompileResult` / `onSourceChanged` /
// `onInputError` / the meter pump.

juce::var compileResultToVar (const CompileResult& result,
                              const ui::Schema*    schemaOrNull)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("success",      result.success);
    obj->setProperty ("crossLoad",    result.crossLoad);
    obj->setProperty ("errorMessage", juce::String (result.errorMessage));
    if (result.errorLine.has_value())
        obj->setProperty ("errorLine", *result.errorLine);
    if (result.errorCol.has_value())
        obj->setProperty ("errorCol",  *result.errorCol);
    obj->setProperty ("numInputs",     result.numInputs);
    obj->setProperty ("numOutputs",    result.numOutputs);
    obj->setProperty ("latencySamples",result.latencySamples);
    obj->setProperty ("svgRoot",
        juce::String (result.svgRoot.getFullPathName()));

    // Schema (annotated with slotIds by ParamBridge::rebindForSchema)
    // — pre-serialised as a JSON string by Core's serializer. We embed
    // it as a parsed object so the JS side gets a structured tree.
    if (schemaOrNull != nullptr)
    {
        const auto serialised = ui::serializeForWebview (*schemaOrNull);
        juce::var parsed;
        if (juce::JSON::parse (juce::String (serialised), parsed).wasOk())
            obj->setProperty ("schema", parsed);
    }
    return juce::var (obj);
}

juce::var sourceChangedToVar (const juce::String& source,
                              const juce::File&   path)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("source", source);
    obj->setProperty ("path",   path.getFullPathName());
    return juce::var (obj);
}

juce::var inputErrorToVar (const SourceLoader::InputError& error)
{
    auto* obj = new juce::DynamicObject();
    const char* kindStr = "unknown";
    switch (error.kind)
    {
        case SourceLoader::InputError::Kind::FileMissing:     kindStr = "fileMissing";    break;
        case SourceLoader::InputError::Kind::FileReadFailed:  kindStr = "fileReadFailed"; break;
        case SourceLoader::InputError::Kind::FileWriteFailed: kindStr = "fileWriteFailed";break;
    }
    obj->setProperty ("kind",    juce::String (kindStr));
    obj->setProperty ("message", error.message);
    return juce::var (obj);
}

juce::var crossLoadErrorToVar (const CompileResult& result)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("kind",       juce::String ("crossLoad"));
    obj->setProperty ("message",    juce::String (result.errorMessage));
    obj->setProperty ("numInputs",  result.numInputs);
    obj->setProperty ("numOutputs", result.numOutputs);
    return juce::var (obj);
}

juce::var compileErrorToVar (const CompileResult& result)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("kind",    juce::String ("compileError"));
    obj->setProperty ("message", juce::String (result.errorMessage));
    if (result.errorLine.has_value())
        obj->setProperty ("errorLine", *result.errorLine);
    if (result.errorCol.has_value())
        obj->setProperty ("errorCol",  *result.errorCol);
    return juce::var (obj);
}

juce::var meterFrameToVar (const MeterFrame& f)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("inL_rms",  f.inL_rms);
    obj->setProperty ("inR_rms",  f.inR_rms);
    obj->setProperty ("inL_peak", f.inL_peak);
    obj->setProperty ("inR_peak", f.inR_peak);
    obj->setProperty ("outL_rms",  f.outL_rms);
    obj->setProperty ("outR_rms",  f.outR_rms);
    obj->setProperty ("outL_peak", f.outL_peak);
    obj->setProperty ("outR_peak", f.outR_peak);

    juce::Array<juce::var> bg;
    bg.resize ((int) f.bargraphValues.size());
    for (int i = 0; i < (int) f.bargraphValues.size(); ++i)
        bg.set (i, f.bargraphValues[(size_t) i]);
    obj->setProperty ("bargraphs", bg);
    return juce::var (obj);
}

#if JUCE_WINDOWS
// Click-on-top: focus the plugin's host holder window. See juce.md
// § Click-on-top in DAWs. Clicking the WebView2 UI doesn't bring the plugin's
// host window forward the way a normal plugin's click does — WebView2's Chromium
// child HWNDs consume the messages the host reacts to. FL Studio is the hard case:
// it nests the editor as WS_CHILD windows under its own TPluginForm wrapper, so
// there is no top-level plugin window to activate (GA_ROOT is FL's already-active
// main window). Fix: walk the peer's parent chain to the TPluginForm holder and
// BringWindowToTop + SetFocus it directly. No-op in hosts without a TPluginForm —
// their top-level plugin windows use the peer's toFront instead.
void focusPluginHolder (HWND peerHwnd)
{
    for (HWND w = peerHwnd; w != nullptr; w = GetParent (w))
    {
        wchar_t cls[64] = {};
        GetClassNameW (w, cls, 63);
        if (juce::String (cls).startsWith ("TPluginForm"))
        {
            BringWindowToTop (w);
            SetFocus (w);
            return;
        }
    }
}
#endif

} // namespace

WebviewBridge::DrainPump::DrainPump (WebviewBridge& ownerIn)
    : owner (ownerIn) {}

void WebviewBridge::DrainPump::timerCallback()
{
    // Free retired DSP instances on the message thread. Combined with
    // the meter drain to avoid two timers in the editor.
    owner.runtime.drainDeletions();

    MeterFrame frame;
    if (! owner.meters.tryPopLatest (frame))
        return;

    if (owner.webView == nullptr)
        return;

    auto json = juce::JSON::toString (meterFrameToVar (frame), true);
    owner.webView->evaluateJavascript (
        "window.crucible && window.crucible.onMeterFrame && window.crucible.onMeterFrame("
        + json + ");",
        {});
}

WebviewBridge::WebviewBridge (juce::AudioProcessorValueTreeState& apvtsIn,
                              FaustRuntime&                       runtimeIn,
                              ParamBridge&                        paramBridgeIn,
                              SourceLoader&                       sourceLoaderIn,
                              MetersBridge&                       metersIn,
                              Variant                             variantIn,
                              CompileResultProvider               getCachedCompileResultIn)
    : apvts (apvtsIn)
    , runtime (runtimeIn)
    , paramBridge (paramBridgeIn)
    , sourceLoader (sourceLoaderIn)
    , meters (metersIn)
    , variant (variantIn)
    , getCachedCompileResult (std::move (getCachedCompileResultIn))
{
    juce::ignoreUnused (apvts, paramBridge);

    juce::WebBrowserComponent::Options options;

   #if JUCE_WINDOWS
    // Per-plugin user-data folder for WebView2 — isolates each plugin
    // instance's WebView2 environment (gain-shaper convention).
    const auto webView2DataFolder = juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)
                                        .getChildFile ("ZCROSSAudio_Crucible_WebView2");
    options = options
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                     .withUserDataFolder (webView2DataFolder));
   #endif

    options = options.withNativeIntegrationEnabled (true)
                     .withUserAgent ("CrucibleVST/0.1");

    // UI-click → focus the plugin's host holder window directly (FL's
    // TPluginForm). The old BringWindowToTop-chain + SetForegroundWindow approach
    // was a no-op in FL (GA_ROOT is FL's already-active main window). See
    // focusPluginHolder + juce.md § Click-on-top in DAWs.
    options = options.withEventListener (juce::Identifier ("crucible.uiClicked"),
        [this] (juce::var)
        {
            if (webView == nullptr)
                return;

            auto* peer = webView->getPeer();
            if (peer == nullptr)
                return;

           #if JUCE_WINDOWS
            if (auto childHwnd = static_cast<HWND> (peer->getNativeHandle()))
                focusPluginHolder (childHwnd);
           #else
            peer->toFront (true);
           #endif
        });

    // Key forwarding to the host. JS captures keydown/keyup at window-
    // capture phase and decides routing:
    //   - Editor surface focused → consume locally
    //   - Clipboard shortcut (Ctrl+C/V/X/A/Z/Y) → consume locally
    //   - everything else → emit `crucible.forwardKey`
    //
    // Universal Win32 pattern (works for any host that consumes the
    // standard OS input queue — i.e. essentially any DAW):
    //   1. Walk to the host's top-level HWND via GetAncestor(GA_ROOT).
    //   2. AttachThreadInput-wrapped SetForegroundWindow + SetFocus on
    //      it. Moves keyboard focus off WebView2 so the synthetic key
    //      doesn't loop back into Chromium.
    //   3. SendInput synthesises the keystroke into the OS input queue.
    //      PostMessage of WM_KEYDOWN does not enter the input queue
    //      and is invisible to host keyboard routers that listen at
    //      the hook chain / input-queue level (which is the standard
    //      mechanism for typing-keyboard routing on Windows).
    options = options.withEventListener (juce::Identifier ("crucible.forwardKey"),
        [this] (juce::var data)
        {
           #if JUCE_WINDOWS
            if (webView == nullptr || ! data.isObject())
                return;

            auto* peer = webView->getPeer();
            if (peer == nullptr)
                return;

            auto childHwnd = static_cast<HWND> (peer->getNativeHandle());
            if (childHwnd == nullptr)
                return;

            HWND host = GetAncestor (childHwnd, GA_ROOT);
            if (host == nullptr)
                host = childHwnd;

            const int  vkCode   = (int)  data.getProperty ("vkCode",   juce::var (0));
            const auto typeStr  = data.getProperty ("type",     juce::var()).toString();
            const bool isDown   =      typeStr == "down";
            const bool extended = (bool) data.getProperty ("extended", juce::var (false));

            if (vkCode <= 0 || vkCode > 0xFE)
                return;

            // Move focus off WebView2 before synthesising the input,
            // otherwise the OS routes the synthetic key back to
            // WebView2's HWND (it still has Win32 focus from the click).
            const DWORD myThread = GetCurrentThreadId();
            const DWORD fgThread = GetWindowThreadProcessId (GetForegroundWindow(), nullptr);
            const BOOL  attached = (myThread != fgThread && fgThread != 0)
                                   ? AttachThreadInput (myThread, fgThread, TRUE)
                                   : FALSE;
            SetForegroundWindow (host);
            SetFocus (host);

            INPUT input{};
            input.type       = INPUT_KEYBOARD;
            input.ki.wVk     = static_cast<WORD> (vkCode);
            input.ki.wScan   = static_cast<WORD> (MapVirtualKey (vkCode, MAPVK_VK_TO_VSC));
            input.ki.dwFlags = (isDown   ? 0u : KEYEVENTF_KEYUP)
                             | (extended ? KEYEVENTF_EXTENDEDKEY : 0u);
            SendInput (1, &input, sizeof (INPUT));

            if (attached)
                AttachThreadInput (myThread, fgThread, FALSE);
           #else
            juce::ignoreUnused (data);
           #endif
        });

    // OPEN — launch file dialog. SourceLoader handles the dialog +
    // file read + watcher start.
    options = options.withEventListener (juce::Identifier ("crucible.openRequest"),
        [this] (juce::var)
        {
            sourceLoader.requestOpen (this);
        });

    // SAVE-As — payload `{ buffer: string }`. SourceLoader opens a
    // Save-As dialog; the watcher picks up the resulting file change
    // on its next poll and emits OnSourceChanged through the normal
    // path.
    options = options.withEventListener (juce::Identifier ("crucible.saveAsRequest"),
        [this] (juce::var data)
        {
            juce::String buffer;
            if (data.isObject())
                buffer = data.getProperty ("buffer", juce::var()).toString();
            else if (data.isString())
                buffer = data.toString();
            sourceLoader.requestSave (buffer, this);
        });

    // Editor buffer changed — payload `{ buffer: string }` or a bare
    // string. Stored in SourceLoader so the Save-As dialog defaults
    // to the user's working edits.
    options = options.withEventListener (juce::Identifier ("crucible.editorBufferChanged"),
        [this] (juce::var data)
        {
            juce::String buffer;
            if (data.isObject())
                buffer = data.getProperty ("buffer", juce::var()).toString();
            else if (data.isString())
                buffer = data.toString();
            sourceLoader.setEditorBuffer (buffer);
        });

    // Replay-state — fired by the JS bridge once on module load. Re-
    // emits onSourceChanged (with the current editor buffer + watched
    // path) and onCompileResult (with the last cached result re-parsed
    // into a schema) so a freshly-mounted webview sees the same state
    // it had before close/reopen. APVTS slot values come through their
    // own attachments and don't need replay here.
    options = options.withEventListener (juce::Identifier ("crucible.replayStateRequest"),
        [this] (juce::var)
        {
            const auto path   = sourceLoader.currentPath();
            const auto buffer = sourceLoader.currentBuffer();
            if (buffer.isNotEmpty() || path != juce::File())
                onSourceChanged (buffer, path);

            if (! getCachedCompileResult)
                return;
            const auto* cached = getCachedCompileResult();
            if (cached == nullptr)
                return;

            std::unique_ptr<ui::Schema> schema;
            if (cached->success && ! cached->jsonUi.empty())
            {
                std::string err;
                schema = ui::parseSchema (cached->jsonUi, &err);
                // Re-annotate the freshly-parsed schema with the
                // existing slotId bindings. ParamBridge's
                // address-to-slot map is already populated (it was set
                // when the original compile callback ran); rebinding
                // on this freshly-parsed schema is idempotent for the
                // bookkeeping — it just writes the existing binding's
                // slotId into each control leaf — but it is REQUIRED
                // for the webview to receive non-empty slotIds. Without
                // this call the UI controls have no APVTS binding on
                // the JS side and slider drags don't reach the DSP.
                if (schema != nullptr)
                    paramBridge.rebindForSchema (*schema);
            }
            onCompileResult (*cached, schema.get());
        });

    // Compile-buffer — debounced trigger from the editor pane. Submits
    // the working buffer directly to the runtime without touching the
    // watched file on disk. Save remains the only disk-write path.
    // Empty buffers are dropped so the user clearing the editor doesn't
    // wipe the running DSP — they keep the last-known-good instance.
    options = options.withEventListener (juce::Identifier ("crucible.compileBufferRequest"),
        [this] (juce::var data)
        {
            juce::String buffer;
            if (data.isObject())
                buffer = data.getProperty ("buffer", juce::var()).toString();
            else if (data.isString())
                buffer = data.toString();
            if (buffer.trim().isEmpty())
                return;
            runtime.submit (buffer.toStdString());
        });

    // COPY CONTEXT — read the bundled skill document and write it to
    // the system clipboard. The document is a v1 placeholder authored
    // in Phase 8.8; replaced as the skill matures.
    options = options.withEventListener (juce::Identifier ("crucible.copyContextRequest"),
        [] (juce::var)
        {
            juce::SystemClipboard::copyTextToClipboard (getCopyContextDocument());
        });

    // COPY — write the editor buffer to the system clipboard through
    // JUCE rather than the webview's `navigator.clipboard`, which fails
    // in WebView2 after the click-on-top handler moves focus off the
    // webview on the (non-editor) button click.
    options = options.withEventListener (juce::Identifier ("crucible.copyToClipboard"),
        [] (juce::var data)
        {
            juce::String text;
            if (data.isObject())
                text = data.getProperty ("text", juce::var()).toString();
            else if (data.isString())
                text = data.toString();
            juce::SystemClipboard::copyTextToClipboard (text);
        });

    // PASTE — read the system clipboard and push it back to JS, which
    // overwrites the editor buffer. Request/response because the webview
    // clipboard-read API is unavailable once focus leaves the webview.
    options = options.withEventListener (juce::Identifier ("crucible.pasteRequest"),
        [this] (juce::var)
        {
            if (webView == nullptr)
                return;
            const auto text = juce::SystemClipboard::getTextFromClipboard();
            const auto json = juce::JSON::toString (juce::var (text), true);
            webView->evaluateJavascript (
                "window.crucible && window.crucible.onClipboardPaste && window.crucible.onClipboardPaste("
                + json + ");",
                {});
        });

    // diagramNavigate — handled in-webview (drill into a Faust SVG
    // subdiagram). Acknowledged here so the JS side can call
    // `juce.emitEvent` without producing a "no listener" warning.
    options = options.withEventListener (juce::Identifier ("crucible.diagramNavigate"),
        [] (juce::var) { /* in-webview state only */ });

    // Register parameter relays for the full slot pool. Each
    // `slot.f.NNN` parameter gets a `WebSliderRelay` + attachment, and
    // each `slot.b.NNN` gets a `WebToggleButtonRelay` + attachment. The
    // JS bridge (`lib/crucible-bridge.ts`) reads/writes by slot id.
    floatRelays.reserve (static_cast<size_t> (kFloatSlotCount));
    floatAttachments.reserve (static_cast<size_t> (kFloatSlotCount));
    for (int i = 0; i < kFloatSlotCount; ++i)
    {
        const auto id = floatSlotId (i);
        auto relay = std::make_unique<juce::WebSliderRelay> (id);
        options = options.withOptionsFrom (*relay);
        if (auto* parameter = apvts.getParameter (id))
            floatAttachments.push_back (
                std::make_unique<juce::WebSliderParameterAttachment> (*parameter, *relay));
        floatRelays.push_back (std::move (relay));
    }

    boolRelays.reserve (static_cast<size_t> (kBoolSlotCount));
    boolAttachments.reserve (static_cast<size_t> (kBoolSlotCount));
    for (int i = 0; i < kBoolSlotCount; ++i)
    {
        const auto id = boolSlotId (i);
        auto relay = std::make_unique<juce::WebToggleButtonRelay> (id);
        options = options.withOptionsFrom (*relay);
        if (auto* parameter = apvts.getParameter (id))
            boolAttachments.push_back (
                std::make_unique<juce::WebToggleButtonParameterAttachment> (*parameter, *relay));
        boolRelays.push_back (std::move (relay));
    }

    // Resource provider — handles both the bundled UI assets (release)
    // and the `/svg/<filename>` lookup (both release and dev). In dev
    // mode the JS bundle comes from Vite at `localhost:5173`, but the
    // SVG fetches still hit the provider via the JUCE-resolved origin.
    const auto variantSlugForProvider = juce::String (variant == Variant::FX ? "fx" : "instrument");
   #if CRUCIBLE_UI_BUNDLED
    options = options.withResourceProvider (
        [variantSlugForProvider] (const juce::String& u)
        { return bundledResourceProvider (u, variantSlugForProvider); });
   #else
    options = options.withResourceProvider (&devSvgResourceProvider);
   #endif

    // Publish our svgRoot accessor pointer so the resource provider
    // lambda can read it. The directory is updated on every successful
    // compile via `onCompileResult`; the atomic publishes the write.
    currentSvgDirPtr.store (&svgRootDir, std::memory_order_release);

    webView = std::make_unique<SinglePageBrowser> (options);

    // Plain navigation matching gain-shaper's working pattern. The
    // variant is injected into index.html by the resource provider so
    // `window.__crucibleVariant` is set before the JS bundle parses.
   #if CRUCIBLE_UI_BUNDLED
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
   #else
    webView->goToURL ("http://localhost:5173/");
   #endif

    addAndMakeVisible (*webView);

    // 30 Hz drain pump — meter frames + retired DSP cleanup.
    drainPump = std::make_unique<DrainPump> (*this);
    drainPump->startTimer (33);
}

WebviewBridge::~WebviewBridge()
{
    // Stop the resource provider from dereferencing `svgRootDir`
    // after this bridge dies. Resource-provider lambdas may still be
    // in flight when the webview's destructor returns; the atomic
    // store ensures any concurrent read sees nullptr and bails out.
    currentSvgDirPtr.store (nullptr, std::memory_order_release);
}

void WebviewBridge::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void WebviewBridge::emit (const juce::String& channel, const juce::String& jsonPayload)
{
    if (webView == nullptr)
        return;

    auto js = juce::String ("window.crucible && window.crucible.")
            + channel + " && window.crucible." + channel + "("
            + jsonPayload + ");";
    webView->evaluateJavascript (js, {});
}

void WebviewBridge::onCompileResult (const CompileResult& result,
                                     const ui::Schema*    schemaOrNull)
{
    // Publish the latest SVG output directory so the resource provider
    // can serve `/svg/<filename>` requests against the new compile's
    // diagram folder. Only update on success — the last-known-good
    // diagram stays browseable through a failed recompile.
    if (result.success && result.svgRoot != juce::File())
        svgRootDir = result.svgRoot;

    // Cross-load failure routes to the input-errors channel as well —
    // the UI's error overlay (Phase 8) surfaces those — but we still
    // emit the compile-result for diagnostic completeness.
    if (! result.success)
    {
        if (result.crossLoad)
            emit ("onInputError", juce::JSON::toString (crossLoadErrorToVar (result), true));
        else
            emit ("onInputError", juce::JSON::toString (compileErrorToVar (result), true));
    }

    emit ("onCompileResult",
          juce::JSON::toString (compileResultToVar (result, schemaOrNull), true));
}

void WebviewBridge::onSourceChanged (const juce::String& source, const juce::File& path)
{
    emit ("onSourceChanged",
          juce::JSON::toString (sourceChangedToVar (source, path), true));
}

void WebviewBridge::onInputError (const SourceLoader::InputError& error)
{
    emit ("onInputError",
          juce::JSON::toString (inputErrorToVar (error), true));
}

} // namespace crucible
