#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

// Global-namespace forward declarations. `::dsp` is the abstract base
// implemented by both plain `llvm_dsp` instances and `mydsp_poly`
// wrappers (via `decorator_dsp`). `currentInstance` is typed as
// `::dsp*` so the same storage holds either form.
class dsp;
class llvm_dsp;
class llvm_dsp_factory;
// libfaust's MIDI event interface (`faust/midi/midi.h`). A polyphonic
// instance (`mydsp_poly`) implements it; a monophonic `llvm_dsp` does
// not. Exposed via `getCurrentMidi` for `MidiBridge` to bind against.
class midi;

namespace crucible {

// Result of a single compile pass through `FaustRuntime::submit`.
// Delivered to the user-supplied callback on the COMPILE WORKER THREAD
// (not the message thread). Callers that need message-thread context
// — e.g. JUCE APVTS or webview interactions — must wrap their work in
// `juce::MessageManager::callAsync` themselves. This keeps the runtime
// usable in unit tests with no MessageManager. See Phase 3 Deviation 3
// in plan-bootstrap-vst.md.
struct CompileResult
{
    bool success = false;

    // Empty on success; libfaust's raw error message on failure.
    std::string errorMessage;

    // Best-effort line:column extraction from libfaust error messages.
    // Both populated together when extraction succeeds; both empty
    // otherwise. Optional so the UI can distinguish "no position
    // available" from "position is 0:0".
    std::optional<int> errorLine;
    std::optional<int> errorCol;

    // True when the failure was caused by the compile predicate
    // rejecting the new instance (cross-load mismatch — e.g. loading
    // a generator-shaped program into Crucible FX). The `errorMessage`
    // carries a user-facing description; `numInputs`/`numOutputs`
    // reflect the rejected program's shape for diagnostics.
    bool crossLoad = false;

    // libfaust's JSON UI description (the output of `factory->getJSON`).
    // Empty on failure.
    std::string jsonUi;

    // Folder containing the `-svg` aux-files output, or an absent
    // juce::File if SVG generation was skipped or failed. The
    // entry-point diagram is `<source-name>-svg/process.svg` inside
    // this folder. Empty on failure.
    juce::File svgRoot;

    int numInputs     = 0;
    int numOutputs    = 0;
    int latencySamples = 0;
};

// FaustRuntime — the compile-and-host engine.
//
// Threading model:
//   - Construction / destruction: any thread (typically the message
//     thread at processor construction time).
//   - submit / setIncludePaths / setSampleRate / setCompileCallback:
//     intended to be called from the message thread. submit coalesces
//     — if the worker is busy, the latest source wins; older
//     submissions are dropped without compilation.
//   - process: AUDIO THREAD only. Lock-free, allocation-free: one
//     atomic load + one `dsp->compute` if the pointer is non-null.
//   - drainDeletions: MESSAGE THREAD only. Frees DSP instances that
//     have been retired by a successful compile.
//   - setZone / getZone: MESSAGE THREAD only. ParamBridge calls these
//     in response to APVTS parameter changes.
//
// The audio thread NEVER deletes, allocates, or locks. Old instances
// are queued for deletion on the message thread.
class FaustRuntime
{
public:
    using OnCompile = std::function<void(const CompileResult&)>;

    FaustRuntime();
    ~FaustRuntime();

    // The callback is invoked on the compile worker thread — wrap in
    // `MessageManager::callAsync` at the call site if you need the
    // message thread (see `CompileResult` docs above).
    void setCompileCallback (OnCompile cb);

    // Variant-specific gating predicate. When set, the worker calls
    // this with the prospective `CompileResult` (factory + instance
    // already built; numInputs / numOutputs populated) BEFORE the
    // atomic swap. If the predicate returns false, the new instance
    // is destroyed, the swap is skipped, and the callback receives a
    // CompileResult with `success = false`, `crossLoad = true`, and
    // a human-readable `errorMessage`.
    //
    // Used by Crucible FX (rejects programs with numInputs == 0) and
    // Crucible Instrument (rejects programs with numInputs > 0). See
    // design-vst.md "Cross-loading rejection".
    using CompilePredicate = std::function<bool (const CompileResult&)>;
    void setCompilePredicate (CompilePredicate p);

    // Include paths passed to libfaust as `-I` args on every compile.
    // Typically the StdlibExtractor's directory plus the watched
    // file's parent.
    void setIncludePaths (const std::vector<juce::File>& paths);

    // Submit a source for compilation. Coalesces — the latest
    // submission wins if the worker is busy.
    void submit (const std::string& source);

    // Sample-rate update. The next compile uses this; if a current
    // instance exists, it is re-init'd at the new rate immediately.
    void setSampleRate (double sr);

    // Enable polyphonic wrapping for subsequent compiles. `maxVoices == 0`
    // (the default) keeps the runtime monophonic — `compileOne` installs
    // the plain `llvm_dsp` returned by the factory. A positive value
    // wraps each freshly-built instance in `mydsp_poly(base, maxVoices,
    // /*control=*/true, /*group=*/true)`. The wrapper owns the base dsp
    // (decorator_dsp pattern) and exposes the `midi` interface, so a
    // `MidiUI` bound via `MidiBridge::bind` will auto-route MIDI note
    // events to the voice-internal `freq` / `gain` / `gate` zones.
    //
    // Must be called BEFORE the first `submit` for it to affect that
    // compile; subsequent compiles pick up the new value.
    void setPolyphony (int maxVoices);

    // AUDIO THREAD: one atomic load + one `dsp->compute` if non-null.
    void process (int numFrames, const float* const* inputs, float* const* outputs);

    // AUDIO THREAD safe. Atomic-load of channel counts published by the
    // worker after a successful swap. The post-swap memory ordering is
    // release/acquire — a non-zero value here implies the currentInstance
    // load that follows will see the swapped-in DSP.
    int currentNumInputs()  const noexcept;
    int currentNumOutputs() const noexcept;

    // MESSAGE THREAD: free retired DSP instances.
    void drainDeletions();

    // MESSAGE THREAD: read/write a Faust UI zone by JSON-UI address.
    // No-op on unknown addresses.
    void setZone (const std::string& address, float value);
    float getZone (const std::string& address) const;

    // MESSAGE THREAD only. Returns the raw `FAUSTFLOAT*` (i.e. `float*`)
    // for the given JSON-UI address in the currently-installed DSP
    // instance, or nullptr if the address is unknown / no instance is
    // installed. ParamBridge caches this pointer at rebind time so the
    // audio thread can read/write zones without taking the runtime mutex.
    //
    // Lifetime: the pointer is valid until the DSP instance is freed.
    // The runtime only frees instances via `drainDeletions` on the
    // message thread, AFTER a subsequent compile-callback has fired —
    // and ParamBridge's rebind (which overwrites cached pointers) runs
    // INSIDE that callback. So as long as cached pointers are refreshed
    // by every successful rebind, they always point into the live (or
    // previous-but-not-yet-deleted) DSP instance — never into freed
    // memory.
    float* getZonePointer (const std::string& address) const;

    // MESSAGE THREAD only. Returns the currently-installed DSP instance
    // pointer (nullptr if no successful compile has landed yet, or the
    // most recent compile failed). Lifetime guarantees match
    // `getZonePointer`: valid until the message thread `drainDeletions`
    // runs after a subsequent successful compile-callback. Used by
    // `MidiBridge::bind` to walk `dsp->buildUserInterface(midiUI)` and
    // register the source's `[midi:...]` mappings.
    ::dsp* getCurrentDsp() const;

    // MESSAGE THREAD only. The current instance's libfaust `::midi`
    // interface when it is polyphonic (an `mydsp_poly` wrapper), or
    // nullptr when monophonic. Passed to `MidiBridge::bind` so note/CC
    // events reach the voice allocator. The type is known statically at
    // wrap time (no RTTI — Faust's prebuilt archive ships llvm_dsp
    // without RTTI on macOS/Linux, so dynamic_cast on it crashes under
    // Clang). Lifetime matches `getCurrentDsp`.
    ::midi* getCurrentMidi() const;

    // Wait for the current submission's compile to finish. Returns
    // immediately if nothing is pending. Returns true if a compile
    // completed within the timeout, false on timeout. Test-only; do
    // not call from production code.
    bool waitForIdle (int timeoutMs);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace crucible
