#pragma once

#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

// Global-namespace forward declaration. The DSP we bind to may be a
// plain `llvm_dsp` (monophonic) or an `mydsp_poly` wrapper; both
// implement the `::dsp` virtual interface and can be passed to
// `MidiUI::buildUserInterface`.
class dsp;

namespace crucible {

// MidiBridge — wraps libfaust's MidiUI glue for the Crucible Instrument
// variant. Each audio block, `process(midiBuffer, dsp)` decodes the
// JUCE MIDI buffer and dispatches events into the libfaust `MidiUI`
// bound to the current DSP instance.
//
// Threading model:
//   - Construction / destruction: any thread (typically message thread
//     at processor construction time).
//   - `bind(dsp)`: MESSAGE THREAD only. Builds a fresh MidiUI for the
//     newly-installed DSP instance, walks `dsp->buildUserInterface` so
//     the MidiUI registers its `[midi:...]` mappings, then atomically
//     publishes the new MidiUI pointer to the audio thread. The
//     previous MidiUI is enqueued for retired-list cleanup on the
//     message thread.
//   - `process(buffer, dsp)`: AUDIO THREAD only. Lock-free,
//     allocation-free: one atomic load + a walk over the MidiBuffer
//     dispatching to the bound MidiUI's `handleKeyOn` / `handleKeyOff`
//     / `handleCtrlChange` / `handleProgChange` / `handlePitchWheel` /
//     `handleAfterTouch` / `handlePolyAfterTouch` methods.
//   - `drainRetired()`: MESSAGE THREAD only. Frees retired MidiUI
//     instances after `bind` has installed a replacement.
//
// The handler's role is split between message and audio threads: the
// audio thread reads the atomic MidiUI* and dispatches events
// directly. The handler's internal `fMidiInputs` vector is NOT used as
// the dispatch path (mutating it requires a lock that the audio thread
// cannot take); instead the atomic pointer is the single source of
// truth for "which MidiUI gets this block's events".
class MidiBridge
{
public:
    MidiBridge();
    ~MidiBridge();

    // MESSAGE THREAD. Bind to a freshly compiled DSP instance. Builds
    // a new MidiUI for `dsp`, walks `dsp->buildUserInterface(midiUI)`
    // to register the [midi:...] mappings declared in the Faust
    // source, and atomically publishes the new MidiUI pointer. The
    // previous MidiUI is retired (freed on the next `drainRetired`).
    //
    // Passing nullptr unbinds — useful on processor teardown.
    void bind (::dsp* dsp);

    // AUDIO THREAD. Decode the JUCE MidiBuffer into the bound MidiUI's
    // event API. Safe to call when no MidiUI is bound — events are
    // simply dropped. No allocations, no locks.
    void process (const juce::MidiBuffer& midiBuffer) noexcept;

    // MESSAGE THREAD. Free any retired MidiUI instances. Should be
    // called on the message-thread side of a compile-callback hop, AFTER
    // `bind` has published a replacement.
    void drainRetired();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace crucible
