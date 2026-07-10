#pragma once

#include <array>
#include <atomic>

#include <juce_core/juce_core.h>

#include "SlotPool.h"

namespace crucible {

// One frame of plugin-level audio meter + Faust bargraph snapshot data
// published by the audio thread to the UI.
//
// Channel reduction (per design-vst.md "Channel handling"):
//  - mono source: both L and R fields carry the same value
//  - 2 channels:  ch 0 -> L, ch 1 -> R
//  - > 2 channels: ch 0 -> L, ch 1 -> R; remaining channels are NOT
//    folded into the meter display (the design contract is a strict
//    two-column display showing channels 0 and 1).
//
// `bargraphValues` is a snapshot of the 16 bargraph readback slot
// atomics (per Phase 5: ParamBridge writes normalised 0..1 values into
// these slots on the audio thread). Unbound slots read as 0.
struct MeterFrame
{
    float inL_rms  = 0.0f;
    float inR_rms  = 0.0f;
    float inL_peak = 0.0f;
    float inR_peak = 0.0f;
    float outL_rms  = 0.0f;
    float outR_rms  = 0.0f;
    float outL_peak = 0.0f;
    float outR_peak = 0.0f;
    std::array<float, kBargraphSlotCount> bargraphValues {};
};

// Single-producer (audio thread) / single-consumer (UI thread)
// lock-free ring of MeterFrames.
//
// Producer: `tryPush` is allocation- and lock-free. If the ring is
// full (UI starved by audio for too long) the frame is dropped — the
// UI only needs the latest reading per tick, so dropping intermediate
// frames is the correct backpressure response.
//
// Consumer: `tryPopLatest` drains every available frame, retaining
// only the most recent. Older frames are discarded — the UI does not
// need block-rate history because its repaint cadence (30 Hz) is
// orders of magnitude slower than the audio block rate.
//
// Ring capacity is 64 frames (gain-shaper convention). At 48 kHz with
// a 256-sample block that's ~340 ms of buffering before drops begin —
// far more than the 33 ms UI tick.
//
// Mirrors gain-shaper's `gainshaper::ui::LiveSignalFifo` shape.
class MetersBridge
{
public:
    static constexpr int kCapacity = 64;

    MetersBridge();

    // AUDIO THREAD. No allocations, no locks. Returns true if the
    // frame was accepted, false if the ring was full and it was dropped.
    bool tryPush (const MeterFrame& frame) noexcept;

    // UI THREAD. Drains every available frame, writing only the most
    // recent to `out`. Returns true if at least one frame was available
    // (and `out` therefore contains a valid frame).
    bool tryPopLatest (MeterFrame& out) noexcept;

private:
    juce::AbstractFifo                  fifo;
    std::array<MeterFrame, kCapacity>   ring {};
};

} // namespace crucible
