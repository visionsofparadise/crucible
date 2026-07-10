#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "FaustRuntime.h"
#include "Schema.h"
#include "SlotPool.h"

namespace crucible {

// ParamBridge — bridges the fixed APVTS slot pool to the dynamic set
// of Faust UI addresses surfaced by the currently-loaded DSP.
//
// On each successful compile, `rebindForSchema` walks the schema and
// either keeps an address's existing slot binding (so DAW automation
// surfaces stay stable across hot reloads) or allocates the next free
// slot of the matching type. Addresses removed from the new schema
// release their slots back to the free pool.
//
// Threading:
//   - All public methods except `readBargraphsIntoSlots` are
//     MESSAGE THREAD only.
//   - `readBargraphsIntoSlots` runs from `processBlock` on the audio
//     thread. It does not lock the bridge's mutex; it reads from a
//     bargraph-specific snapshot updated under `rebindForSchema` and
//     never touches APVTS via `setValueNotifyingHost`. Bargraph values
//     are stored directly into cached `std::atomic<float>*` parameter
//     pointers, bypassing listeners (which is the only audio-thread-safe
//     way to update a host-visible parameter from `processBlock`).
//
// APVTS listener path:
//   - At construction, ParamBridge registers itself as a listener for
//     every float/bool slot ID (not the bargraph slots — those are
//     write-only from this side).
//   - `parameterChanged` looks up the slot ID's bound Faust address;
//     if bound, denormalises the value per the control's min/max and
//     forwards to `runtime.setZone(address, denormalised)`.
//
// Persistence:
//   - `saveMapping` / `restoreMapping` serialise the address→slot map
//     into a juce::ValueTree the PluginProcessor stores alongside APVTS
//     state. On a project reload, restore the mapping BEFORE the first
//     compile so the eventual `rebindForSchema` keeps the persisted
//     bindings.
class ParamBridge : private juce::AudioProcessorValueTreeState::Listener
{
public:
    ParamBridge (juce::AudioProcessorValueTreeState& apvts,
                 FaustRuntime&                        runtime);
    ~ParamBridge() override;

    // MESSAGE THREAD. Called on each successful compile with the parsed
    // schema. Updates the address-to-slot map, seeds the new DSP
    // instance with current APVTS values for every bound address, and
    // annotates each control leaf in the schema with its bound slotId
    // (empty string if unbound due to over-budget).
    void rebindForSchema (ui::Schema& schema);

    // AUDIO THREAD. Reads bargraph zones from the runtime's current
    // map and stores the normalised value into the cached atomic
    // pointer of each bargraph slot. No allocations, no locks.
    void readBargraphsIntoSlots() noexcept;

    // AUDIO THREAD. Snapshot the current bargraph readback slot values
    // into `out`. Slot index N maps to `out[N]`. Unbound or
    // not-yet-published bargraph slots read as 0. No allocations,
    // no locks — reads from the per-slot APVTS atomics cached at
    // construction. Intended for `MetersBridge::tryPush` per block.
    void snapshotBargraphsInto (std::array<float, kBargraphSlotCount>& out) const noexcept;

    // MESSAGE THREAD. Snapshot of the address-to-slot map into a
    // ValueTree the PluginProcessor stores alongside APVTS state.
    juce::ValueTree saveMapping() const;

    // MESSAGE THREAD. Restore a previously-saved mapping. Call before
    // the first compile so subsequent rebinds preserve the bindings.
    void restoreMapping (const juce::ValueTree& tree);

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace crucible
