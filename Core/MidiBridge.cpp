#include "MidiBridge.h"

#include <atomic>
#include <vector>

// libfaust headers (already on Crucible's include path via the faust
// FetchContent). MidiUI brings in midi.h transitively. llvm-dsp pulls
// in the full llvm_dsp type required to call `buildUserInterface`.
#include <faust/gui/MidiUI.h>
#include <faust/midi/midi.h>
#include <faust/dsp/dsp.h>
#include <faust/dsp/llvm-dsp.h>

// libfaust's `GUI` base class declares two static members
// (`fGuiList`, `gTimedZoneMap`) that the architecture file is
// expected to define in exactly one translation unit. We are that
// TU for Crucible — MidiUI inherits from GUI, so without these
// definitions every consumer linking against our Core gets
// `LNK2001: unresolved external` for both symbols. Faust's
// shipped architectures (juce-midi.h, etc.) do this same trick
// in their own one-TU pattern.
std::list<GUI*>      GUI::fGuiList;
ztimedmap            GUI::gTimedZoneMap;

namespace crucible {

namespace
{
    // Minimal midi_handler subclass. libfaust's `MidiUI` constructor
    // calls `handler->addMidiIn(this)`; we satisfy the API surface but
    // we never use the handler's `fMidiInputs` vector as the dispatch
    // path — the audio thread reads our atomic MidiUI pointer and
    // dispatches events directly.
    class CrucibleMidiHandler : public midi_handler
    {
    public:
        CrucibleMidiHandler() : midi_handler ("Crucible") {}
        ~CrucibleMidiHandler() override = default;
    };

    // One "bound" snapshot: the MidiUI owns the [midi:...] declarative
    // mapping tables, and `polyMidi` is a non-owning pointer to the
    // DSP's `midi` interface (the `mydsp_poly` wrapper) — used to
    // deliver keyOn / keyOff / etc. directly to the voice allocator.
    // Without the direct poly dispatch, NoteOn never reaches the
    // `freq/gain/gate` voice convention because `MidiUI::keyOn` only
    // updates its OWN declarative tables — it does NOT broadcast to
    // the handler's registered listeners. The poly's `addMidiIn(this)`
    // registration is therefore unused for audio-thread dispatch; we
    // route around it to keep the audio thread out of the handler's
    // `fMidiInputs` vector (which the message thread mutates on every
    // bind / retire).
    struct Bound
    {
        std::unique_ptr<MidiUI> ui;       // owned
        ::midi*                 polyMidi = nullptr; // non-owning; lifetime tied to the DSP
                                                    // pointer the caller passed to bind()
    };
}

struct MidiBridge::Impl
{
    CrucibleMidiHandler              handler;

    // Atomic pointer to the currently-installed Bound. Audio thread
    // `load`s; message thread `store`s on bind. Lifetime: the Bound is
    // owned by `live`; previous Bound objects are moved to `retired`
    // for message-thread cleanup after a subsequent `bind` has
    // published a new pointer.
    std::atomic<Bound*>              bound { nullptr };

    std::unique_ptr<Bound>           live;
    std::vector<std::unique_ptr<Bound>> retired;
};

MidiBridge::MidiBridge()
    : impl (std::make_unique<Impl>())
{
}

MidiBridge::~MidiBridge()
{
    // Drop the audio-thread-visible pointer before destroying live /
    // retired Bound objects.
    impl->bound.store (nullptr, std::memory_order_release);
    impl->retired.clear();
    impl->live.reset();
}

void MidiBridge::bind (::dsp* dsp, ::midi* midiInterface)
{
    // Build a fresh Bound: a new MidiUI for the [midi:...] declarative
    // mappings plus a non-owning pointer to the DSP's `midi` interface
    // (the `mydsp_poly` wrapper) so the audio thread can deliver MIDI
    // events directly to the voice allocator.
    auto next = std::make_unique<Bound>();
    if (dsp != nullptr)
    {
        next->ui = std::make_unique<MidiUI> (&impl->handler);
        // Walk `buildUserInterface(midiUI)` so the MidiUI's tables get
        // populated from the source's `declare(..., "midi", "ctrl N")`
        // metadata. The poly's `buildUserInterface` also registers
        // itself with the MidiUI / handler — we route around that for
        // dispatch but the walk is still required for the declarative
        // mapping side.
        dsp->buildUserInterface (next->ui.get());
        // Direct dispatch target, supplied by the caller. Non-null only
        // for a polyphonic (`mydsp_poly`) instance — the runtime knows
        // the wrapper type statically and upcasts to `::midi` without
        // RTTI; for a monophonic `llvm_dsp` this is nullptr and only the
        // MidiUI declarative path runs.
        next->polyMidi = midiInterface;
    }

    // Atomically publish the new pointer.
    Bound* raw = next.get();
    impl->bound.store (raw, std::memory_order_release);

    // Retire the previous Bound; cleanup deferred to `drainRetired` on
    // the message thread.
    if (impl->live)
        impl->retired.push_back (std::move (impl->live));

    impl->live = std::move (next);
}

void MidiBridge::process (const juce::MidiBuffer& midiBuffer) noexcept
{
    // AUDIO THREAD. One atomic load + a buffer walk; no allocations,
    // no locks.
    auto* bound = impl->bound.load (std::memory_order_acquire);
    if (bound == nullptr)
        return;

    auto* midiUi   = bound->ui.get();
    auto* polyMidi = bound->polyMidi;

    // We dispatch every event to BOTH targets:
    //   - the MidiUI, which uses its internal tables built from the
    //     source's `declare(..., "midi", "ctrl N")` etc. metadata to
    //     update directly-mapped zones;
    //   - the poly DSP's `midi` interface (the `mydsp_poly` wrapper),
    //     which interprets keyOn / keyOff / pitchWheel / etc. against
    //     the `freq/gain/gate` voice convention to allocate voices.
    //
    // We deliberately do NOT route through `handler.handleKeyOn` (the
    // libfaust broadcast path): that would iterate
    // `handler.fMidiInputs`, which the message thread mutates on every
    // bind / retire / drainRetired via the MidiUI / poly self-
    // registrations. Iterating that vector on the audio thread is
    // unsafe without a lock; the dual dispatch here keeps the audio
    // thread out of it entirely.
    for (const auto metadata : midiBuffer)
    {
        const auto& msg = metadata.getMessage();
        // `time` is the sample offset within the current block.
        // libfaust accepts double-precision timestamps; sample offsets
        // serialise naturally.
        const double time = (double) metadata.samplePosition;
        const auto* data  = msg.getRawData();
        const int channel = msg.getChannel() - 1; // 0..15 in libfaust's layer

        if (msg.isNoteOff())
        {
            midiUi->keyOff (time, channel, data[1], data[2]);
            if (polyMidi != nullptr)
                polyMidi->keyOff (time, channel, data[1], data[2]);
        }
        else if (msg.isNoteOn())
        {
            // libfaust convention: NoteOn with velocity 0 == NoteOff.
            if (data[2] == 0)
            {
                midiUi->keyOff (time, channel, data[1], 0);
                if (polyMidi != nullptr)
                    polyMidi->keyOff (time, channel, data[1], 0);
            }
            else
            {
                midiUi->keyOn (time, channel, data[1], data[2]);
                if (polyMidi != nullptr)
                    polyMidi->keyOn (time, channel, data[1], data[2]);
            }
        }
        else if (msg.isAftertouch())
        {
            midiUi->keyPress (time, channel, data[1], data[2]);
            if (polyMidi != nullptr)
                polyMidi->keyPress (time, channel, data[1], data[2]);
        }
        else if (msg.isController())
        {
            midiUi->ctrlChange (time, channel, data[1], data[2]);
            if (polyMidi != nullptr)
                polyMidi->ctrlChange (time, channel, data[1], data[2]);
        }
        else if (msg.isProgramChange())
        {
            midiUi->progChange (time, channel, msg.getProgramChangeNumber());
            if (polyMidi != nullptr)
                polyMidi->progChange (time, channel, msg.getProgramChangeNumber());
        }
        else if (msg.isChannelPressure())
        {
            midiUi->chanPress (time, channel, data[1]);
            if (polyMidi != nullptr)
                polyMidi->chanPress (time, channel, data[1]);
        }
        else if (msg.isPitchWheel())
        {
            midiUi->pitchWheel (time, channel, msg.getPitchWheelValue());
            if (polyMidi != nullptr)
                polyMidi->pitchWheel (time, channel, msg.getPitchWheelValue());
        }
        else if (msg.isMidiClock())
        {
            midiUi->clock (time);
            if (polyMidi != nullptr)
                polyMidi->clock (time);
        }
        else if (msg.isMidiStart() || msg.isMidiContinue())
        {
            midiUi->startSync (time);
            if (polyMidi != nullptr)
                polyMidi->startSync (time);
        }
        else if (msg.isMidiStop())
        {
            midiUi->stopSync (time);
            if (polyMidi != nullptr)
                polyMidi->stopSync (time);
        }
        // SysEx and other less-common messages are dropped silently —
        // libfaust's MidiUI doesn't surface them to user zones.
    }
}

void MidiBridge::drainRetired()
{
    // MESSAGE THREAD. Free retired MidiUIs. Safe because every retired
    // MidiUI was published-then-replaced by a subsequent `bind`, and
    // any audio-thread reader observing the old pointer has long since
    // finished its block.
    impl->retired.clear();
}

} // namespace crucible
