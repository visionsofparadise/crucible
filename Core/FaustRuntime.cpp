#include "FaustRuntime.h"

#include <faust/dsp/llvm-dsp.h>
#include <faust/dsp/poly-dsp.h>
#include <faust/gui/JSONUI.h>
#include <faust/gui/MapUI.h>

#include <algorithm>
#include <regex>
#include <utility>

// `generateAuxFilesFromString` lives in compiler/generator/libfaust.h,
// not under faust/dsp/. Declare it locally rather than depending on
// the compiler-internal header path, which isn't on the include line
// for downstream targets. The libfaustwithllvm static archive exports
// it as a C++ symbol (no `extern "C"`).
bool generateAuxFilesFromString (const std::string& name_app,
                                 const std::string& dsp_content,
                                 int                argc,
                                 const char*        argv[],
                                 std::string&       error_msg);

namespace crucible {

namespace
{
    void parseLineCol (const std::string& msg, std::optional<int>& line, std::optional<int>& col)
    {
        // Faust errors typically include something like
        // "ERROR : foo.dsp:12:7" — pick the first `N:N` occurrence.
        static const std::regex re (R"((\d+)\s*:\s*(\d+))");
        std::smatch m;
        if (std::regex_search (msg, m, re) && m.size() >= 3)
        {
            try
            {
                line = std::stoi (m[1].str());
                col  = std::stoi (m[2].str());
            }
            catch (...)
            {
                line.reset();
                col.reset();
            }
        }
    }

    juce::File makeSvgTempDir()
    {
        auto base = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("ZCROSS")
                        .getChildFile ("Crucible")
                        .getChildFile ("svg");
        base.createDirectory();
        auto unique = base.getChildFile (juce::Uuid().toDashedString());
        unique.createDirectory();
        return unique;
    }
}

struct FaustRuntime::Impl
{
    Impl()
    {
        worker = std::thread ([this] { workerLoop(); });
    }

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock (mutex);
            shouldExit = true;
        }
        cv.notify_all();
        if (worker.joinable())
            worker.join();

        // Release current instance + factory + retired instances.
        // `currentInstance` is typed as `::dsp*` because it may hold
        // either a plain `llvm_dsp` (monophonic) or an `mydsp_poly`
        // wrapper. `delete` on the base virtual destructor runs the
        // correct derived destructor in both cases; the poly wrapper's
        // destructor in turn frees its base dsp via the decorator
        // pattern.
        if (auto* d = currentInstance.exchange (nullptr))
            delete d;
        currentVoiceGroup = nullptr;
        for (auto* d : retired)
            delete d;
        retired.clear();
        if (currentMap != nullptr)
        {
            delete currentMap;
            currentMap = nullptr;
        }
        if (currentFactory != nullptr)
        {
            deleteDSPFactory (currentFactory);
            currentFactory = nullptr;
        }
        for (const auto& dir : retiredSvgDirs)
            dir.deleteRecursively();
        retiredSvgDirs.clear();
        if (currentSvgDir != juce::File())
        {
            currentSvgDir.deleteRecursively();
            currentSvgDir = {};
        }
    }

    void submitSource (std::string src)
    {
        {
            std::lock_guard<std::mutex> lock (mutex);
            pendingSource = std::move (src);
            ++submitSeq;
        }
        cv.notify_all();
    }

    void workerLoop()
    {
        while (true)
        {
            std::string source;
            std::vector<std::string> includes;
            double sr;
            {
                std::unique_lock<std::mutex> lock (mutex);
                cv.wait (lock, [this] { return shouldExit || pendingSource.has_value(); });
                if (shouldExit)
                    return;

                source = std::move (*pendingSource);
                pendingSource.reset();
                includes = includePathsCopy;
                sr = sampleRate;
                workerBusy = true;
            }

            CompileResult result = compileOne (source, includes, sr);

            {
                std::lock_guard<std::mutex> lock (mutex);
                workerBusy = false;
                ++completeSeq;
            }
            idleCv.notify_all();

            // Invoke the callback outside the lock. Snapshot the
            // callback under the lock to avoid races with setCompileCallback.
            OnCompile cb;
            {
                std::lock_guard<std::mutex> lock (mutex);
                cb = callback;
            }
            if (cb)
                cb (result);
        }
    }

    CompileResult compileOne (const std::string& source,
                              const std::vector<std::string>& includes,
                              double sr)
    {
        CompileResult out;

        std::vector<std::string> argStorage;
        argStorage.reserve (includes.size() * 2);
        for (const auto& inc : includes)
        {
            argStorage.emplace_back ("-I");
            argStorage.emplace_back (inc);
        }
        std::vector<const char*> argv;
        argv.reserve (argStorage.size());
        for (const auto& s : argStorage)
            argv.push_back (s.c_str());

        std::string err;
        llvm_dsp_factory* factory = createDSPFactoryFromString (
            "crucible",
            source,
            static_cast<int> (argv.size()),
            argv.empty() ? nullptr : argv.data(),
            "", err, -1);

        if (factory == nullptr)
        {
            out.success      = false;
            out.errorMessage = err;
            parseLineCol (out.errorMessage, out.errorLine, out.errorCol);
            return out;
        }

        llvm_dsp* base = factory->createDSPInstance();
        if (base == nullptr)
        {
            out.success      = false;
            out.errorMessage = "createDSPInstance returned nullptr";
            deleteDSPFactory (factory);
            return out;
        }

        // Snapshot polyphony under the mutex — setPolyphony may race
        // with compileOne otherwise (compiles run on the worker thread,
        // setPolyphony is intended to be set from the message thread
        // before the first compile but we don't want to assume that).
        int polyphonySnapshot = 0;
        {
            std::lock_guard<std::mutex> lock (mutex);
            polyphonySnapshot = polyphony;
        }

        // Wrap the base dsp in `mydsp_poly` when polyphony is enabled.
        // The wrapper takes ownership of `base` and deletes it via its
        // own destructor (decorator_dsp pattern); we must NOT delete
        // `base` separately. With `control=true`, voices are
        // dynamically allocated from MIDI events; with `group=true`,
        // shared and voice params are funnelled through the GroupUI so
        // a single control drives all voices.
        //
        // `mydsp_poly` exposes the libfaust `midi` interface; when
        // `MidiBridge` walks `buildUserInterface(midiUI)` on this
        // pointer, `dynamic_cast<midi_interface*>` succeeds and the
        // wrapper registers itself for note routing — that's the whole
        // point of this wrapping.
        ::dsp* dsp = (polyphonySnapshot > 0)
            ? static_cast<::dsp*> (new mydsp_poly (base, polyphonySnapshot, /*control=*/true, /*group=*/true))
            : static_cast<::dsp*> (base);

        const int initSr = (sr > 0.0) ? static_cast<int> (sr) : 48000;
        // Init the outermost wrapper. `mydsp_poly::init` propagates
        // through to every voice (clones included).
        dsp->init (initSr);

        auto* zoneMap = new MapUI();
        // Build MapUI from the (possibly-wrapped) instance. In the
        // poly case, addresses get prefixed with `/Polyphonic/Voices/`
        // by `dsp_voice_group::buildUserInterface`.
        dsp->buildUserInterface (zoneMap);

        out.success      = true;
        out.numInputs    = dsp->getNumInputs();
        out.numOutputs   = dsp->getNumOutputs();
        out.latencySamples = 0;

        // Schema JSON. The MapUI we just built came from the
        // (possibly-wrapped) `dsp`, so its addresses are
        // `/Polyphonic/Voices/<root>/...` in poly mode. We must
        // publish a schema with matching addresses, otherwise
        // ParamBridge's lookups silently no-op and UI sliders won't
        // drive the zones.
        //
        // `factory->getJSON()` describes the unwrapped DSP — correct
        // for monophonic, wrong for poly. `dsp_poly::getJSON()`
        // exists but is gated behind `#ifdef EMCC` in poly-dsp.h, so
        // outside an EMCC build it falls back to the default
        // `JSONControl::getJSON()` which returns an empty string —
        // unusable for our schema parser.
        //
        // The robust path is to walk the wrapped instance with
        // `JSONUI` ourselves — this is exactly what `dsp_poly`'s
        // EMCC constructor does (poly-dsp.h:437-439), and it shares
        // the same `JSONUIReal` serializer underlying
        // `factory->getJSON()`, so the produced JSON is
        // schema-parser-compatible. Addresses come from the same
        // `buildUserInterface` walk we used to populate the MapUI,
        // so the two views agree by construction.
        if (polyphonySnapshot > 0)
        {
            JSONUI jsonui (dsp->getNumInputs(), dsp->getNumOutputs());
            dsp->buildUserInterface (&jsonui);
            out.jsonUi = jsonui.JSON (true);
        }
        else
        {
            out.jsonUi = factory->getJSON();
        }

        // Variant predicate (cross-load gate). Evaluated BEFORE the
        // atomic swap so a rejected instance never reaches the audio
        // thread. The predicate sees numInputs/numOutputs already
        // populated; on rejection, destroy the instance + factory
        // and surface a crossLoad error.
        FaustRuntime::CompilePredicate predicateSnapshot;
        {
            std::lock_guard<std::mutex> lock (mutex);
            predicateSnapshot = predicate;
        }
        if (predicateSnapshot && ! predicateSnapshot (out))
        {
            delete dsp;
            delete zoneMap;
            deleteDSPFactory (factory);
            CompileResult rejected;
            rejected.success      = false;
            rejected.crossLoad    = true;
            rejected.errorMessage = "Faust source variant mismatch — "
                                    "this program is incompatible with the "
                                    "current plugin variant ("
                                  + std::to_string (out.numInputs) + " in / "
                                  + std::to_string (out.numOutputs) + " out).";
            rejected.numInputs    = out.numInputs;
            rejected.numOutputs   = out.numOutputs;
            return rejected;
        }

        // SVG aux-files generation. Best-effort: failures don't fail
        // the compile, but svgRoot will be an empty juce::File.
        auto svgTempDir = makeSvgTempDir();
        {
            const std::string outPath = svgTempDir.getFullPathName().toStdString();
            std::vector<std::string> svgArgsStorage;
            svgArgsStorage.reserve (argStorage.size() + 4);
            for (const auto& a : argStorage)
                svgArgsStorage.push_back (a);
            svgArgsStorage.emplace_back ("-svg");
            svgArgsStorage.emplace_back ("-O");
            svgArgsStorage.emplace_back (outPath);
            std::vector<const char*> svgArgv;
            svgArgv.reserve (svgArgsStorage.size());
            for (const auto& s : svgArgsStorage)
                svgArgv.push_back (s.c_str());

            std::string svgErr;
            const bool ok = generateAuxFilesFromString (
                "crucible", source,
                static_cast<int> (svgArgv.size()),
                svgArgv.empty() ? nullptr : svgArgv.data(),
                svgErr);
            if (ok)
                out.svgRoot = svgTempDir;
            else
                svgTempDir.deleteRecursively();
        }

        // Install the new instance gaplessly. Retire the old one.
        //
        // Memory ordering note: the channel-count atomics are stored
        // BEFORE the currentInstance exchange (which is itself a
        // release-acquire fence via the atomic op). The audio thread
        // reads `currentNumInputs/Outputs` with acquire ordering and
        // `currentInstance` with acquire ordering — either it sees the
        // old instance + old counts, or the new instance + new counts.
        // It can briefly see new counts + old instance (counts updated,
        // pointer not yet swapped), which is harmless: the audio thread
        // uses counts only to size pointer-array loops; one block of
        // mismatched shape is invisible because Faust's compute uses
        // its own internal numInputs/numOutputs.
        {
            std::lock_guard<std::mutex> lock (mutex);
            currentNumIns.store  (out.numInputs,  std::memory_order_release);
            currentNumOuts.store (out.numOutputs, std::memory_order_release);

            if (auto* prev = currentInstance.exchange (dsp, std::memory_order_acq_rel))
                retired.push_back (prev);
            if (currentMap != nullptr)
            {
                delete currentMap;
            }
            currentMap = zoneMap;
            // Cache the voice-group base so `setZone` can fan out
            // grouped-control writes to every voice. dynamic_cast yields
            // nullptr in the monophonic case (plain llvm_dsp does not
            // derive from dsp_voice_group); setZone then skips fan-out.
            currentVoiceGroup = dynamic_cast<dsp_voice_group*> (dsp);
            if (currentFactory != nullptr)
            {
                // Defer factory deletion until after instance retire
                // drains, since instances are tied to their factory.
                retiredFactories.push_back (currentFactory);
            }
            currentFactory = factory;

            if (out.svgRoot != juce::File())
            {
                // Note: the previous svg dir cannot be deleted yet
                // because the message thread may still be reading it
                // for the UI. Queue for later cleanup; drainDeletions
                // sweeps it.
                if (currentSvgDir != juce::File())
                    retiredSvgDirs.push_back (currentSvgDir);
                currentSvgDir = out.svgRoot;
            }
        }

        return out;
    }

    void doDrainDeletions()
    {
        std::vector<::dsp*> toDelete;
        std::vector<llvm_dsp_factory*> factoriesToDelete;
        std::vector<juce::File> dirsToDelete;
        {
            std::lock_guard<std::mutex> lock (mutex);
            toDelete.swap (retired);
            factoriesToDelete.swap (retiredFactories);
            dirsToDelete.swap (retiredSvgDirs);
        }
        // `dsp`'s destructor is virtual; deleting through the base
        // pointer runs the correct derived destructor (and the poly
        // wrapper's destructor frees its base dsp transitively). The
        // factory is retired AFTER the instances that depend on it
        // (mydsp_poly clones the base, so all clones go away when the
        // wrapper is destroyed — before we free the factory).
        for (auto* d : toDelete)
            delete d;
        for (auto* f : factoriesToDelete)
            deleteDSPFactory (f);
        for (const auto& dir : dirsToDelete)
            dir.deleteRecursively();
    }

    std::thread worker;
    std::mutex  mutex;
    std::condition_variable cv;
    std::condition_variable idleCv;

    // Producer state, guarded by mutex.
    std::optional<std::string> pendingSource;
    std::vector<std::string>   includePathsCopy;
    double                     sampleRate  = 0.0;
    bool                       shouldExit  = false;
    bool                       workerBusy  = false;
    juce::uint64               submitSeq   = 0;
    juce::uint64               completeSeq = 0;
    OnCompile                       callback;
    FaustRuntime::CompilePredicate  predicate;

    // Current-instance state. Typed as `::dsp*` because `mydsp_poly`
    // (when polyphony is enabled) is not an `llvm_dsp` — only the
    // common `dsp` base — but exposes the virtual `init` / `compute`
    // / `buildUserInterface` we use from the audio + message threads.
    std::atomic<::dsp*>    currentInstance { nullptr };
    llvm_dsp_factory*      currentFactory = nullptr;
    MapUI*                 currentMap     = nullptr;
    // Non-null when the current instance is polyphonic. Points at the
    // wrapping `mydsp_poly`'s `dsp_voice_group` base, which exposes
    // `fGroups` (a `GroupUI`) — the only mechanism that fans out a
    // shared-control write to every voice when `group=true`. MapUI's
    // raw zone writes only touch voice 0 (the prototype walked by
    // `dsp_voice_group::buildUserInterface`), so without this fan-out
    // poly slider drags never affect the other voices. Lifetime tied
    // to `currentInstance` — refreshed in compileOne, cleared when no
    // instance is installed.
    dsp_voice_group*       currentVoiceGroup = nullptr;
    juce::File             currentSvgDir;
    // Polyphony setting; 0 = monophonic (current behaviour). Mutated
    // via setPolyphony under the runtime mutex; snapshotted in
    // compileOne for the wrap decision.
    int                    polyphony      = 0;
    // Channel counts published atomically by the worker after the swap.
    // Audio-thread reads via `currentNumInputs()` / `currentNumOutputs()`
    // do NOT take the mutex — they `load` here. Stored with release
    // ordering before the `currentInstance` store so any audio-thread
    // observer that sees the new instance also sees the new counts.
    std::atomic<int>       currentNumIns  { 0 };
    std::atomic<int>       currentNumOuts { 0 };

    // Retired state, drained from the message thread.
    std::vector<::dsp*>             retired;
    std::vector<llvm_dsp_factory*>  retiredFactories;
    std::vector<juce::File>         retiredSvgDirs;
};

FaustRuntime::FaustRuntime()
    : impl (std::make_unique<Impl>())
{
}

FaustRuntime::~FaustRuntime() = default;

void FaustRuntime::setCompileCallback (OnCompile cb)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->callback = std::move (cb);
}

void FaustRuntime::setCompilePredicate (CompilePredicate p)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->predicate = std::move (p);
}

void FaustRuntime::setIncludePaths (const std::vector<juce::File>& paths)
{
    std::vector<std::string> asStrings;
    asStrings.reserve (paths.size());
    for (const auto& p : paths)
        asStrings.push_back (p.getFullPathName().toStdString());

    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->includePathsCopy = std::move (asStrings);
}

void FaustRuntime::submit (const std::string& source)
{
    impl->submitSource (source);
}

void FaustRuntime::setSampleRate (double sr)
{
    // Hold the mutex across the dsp->init() call. This prevents the
    // compile worker from swapping `currentInstance` out from under us
    // and from deleting the prior instance via the retired-queue path
    // while init is running. Callers must guarantee the audio thread
    // is stopped (JUCE prepareToPlay contract); the mutex covers
    // worker/init contention which can still occur concurrently.
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->sampleRate = sr;
    // `init` is virtual on `dsp`; for `mydsp_poly` this propagates
    // through to every voice clone (see `dsp_voice_group::init` and
    // `decorator_dsp::init`).
    auto* d = impl->currentInstance.load (std::memory_order_acquire);
    if (d != nullptr && sr > 0.0)
        d->init (static_cast<int> (sr));
}

void FaustRuntime::setPolyphony (int maxVoices)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    impl->polyphony = (maxVoices > 0) ? maxVoices : 0;
}

void FaustRuntime::process (int numFrames, const float* const* inputs, float* const* outputs)
{
    auto* d = impl->currentInstance.load (std::memory_order_acquire);
    if (d == nullptr)
        return;
    d->compute (numFrames,
                const_cast<float**> (inputs),
                const_cast<float**> (outputs));
}

int FaustRuntime::currentNumInputs() const noexcept
{
    return impl->currentNumIns.load (std::memory_order_acquire);
}

int FaustRuntime::currentNumOutputs() const noexcept
{
    return impl->currentNumOuts.load (std::memory_order_acquire);
}

void FaustRuntime::drainDeletions()
{
    impl->doDrainDeletions();
}

void FaustRuntime::setZone (const std::string& address, float value)
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    if (impl->currentMap == nullptr)
        return;

    // Direct zone write: MapUI dereferences the FAUSTFLOAT* it cached
    // when `dsp->buildUserInterface(mapUI)` walked the wrapped instance.
    // In the polyphonic+group=true case, `dsp_voice_group::buildUserInterface`
    // walks `fVoiceGroup` (a proxy_dsp around voice 0), so the cached
    // pointer is voice 0's zone — a raw write touches voice 0 only.
    impl->currentMap->setParamValue (address, value);

    // Fan out to every voice via the GroupUI. The GroupUI registered
    // a `uiGroupItem` for each shared-control label during `init()`,
    // holding voice 0's zone as the primary `fZone` and the other
    // voices' zones in its internal vector. Calling `updateAllZones`
    // iterates each registered zone and, when its current value
    // differs from the cached value, fires `reflectZone` — which
    // copies the primary value into every sibling voice zone. This
    // is the standard GUI-refresh path Faust uses for live UIs;
    // we drive it manually here because writes arrive via APVTS
    // listeners, not through any `uiItem::modifyZone` callback.
    //
    // Cost: O(num_shared_controls) per setZone call. Message thread
    // only, so the constant factor is irrelevant compared with
    // dispatch overhead.
    if (impl->currentVoiceGroup != nullptr)
        impl->currentVoiceGroup->fGroups.updateAllZones();
}

float FaustRuntime::getZone (const std::string& address) const
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    if (impl->currentMap != nullptr)
        return impl->currentMap->getParamValue (address);
    return 0.0f;
}

float* FaustRuntime::getZonePointer (const std::string& address) const
{
    // Message-thread only. Returns the raw zone pointer from the
    // currently-installed MapUI, or nullptr if the address is unknown.
    // See header for lifetime rules.
    std::lock_guard<std::mutex> lock (impl->mutex);
    if (impl->currentMap == nullptr)
        return nullptr;
    return impl->currentMap->getParamZone (address);
}

::dsp* FaustRuntime::getCurrentDsp() const
{
    // Message-thread only. The atomic load is cheap; we don't need the
    // mutex because the instance pointer's lifetime is governed by the
    // retire-on-compile + drainDeletions contract (see header).
    return impl->currentInstance.load (std::memory_order_acquire);
}

bool FaustRuntime::waitForIdle (int timeoutMs)
{
    // Idle = no source pending in the slot AND worker is not actively
    // compiling. Coalesced submissions collapse to a single completion,
    // so we cannot reason about idleness by counting submits.
    std::unique_lock<std::mutex> lock (impl->mutex);
    return impl->idleCv.wait_for (
        lock, std::chrono::milliseconds (timeoutMs),
        [this] { return ! impl->pendingSource.has_value() && ! impl->workerBusy; });
}

} // namespace crucible
