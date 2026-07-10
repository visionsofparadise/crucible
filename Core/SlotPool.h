#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace crucible {

// Slot pool sizes — see design-vst.md "APVTS pre-allocated parameter
// slot pool" decision.
inline constexpr int kFloatSlotCount    = 128;
inline constexpr int kBoolSlotCount     = 32;   // slot.b.000 reserved as bypass
inline constexpr int kBargraphSlotCount = 16;
inline constexpr int kBypassSlotIndex   = 0;

// Stable parameter IDs.
juce::String floatSlotId    (int index); // "slot.f.012"
juce::String boolSlotId     (int index); // "slot.b.003"
juce::String bargraphSlotId (int index); // "slot.m.005"

// Build the full APVTS layout once at processor construction.
juce::AudioProcessorValueTreeState::ParameterLayout makeSlotPoolLayout();

} // namespace crucible
