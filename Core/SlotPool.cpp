#include "SlotPool.h"

namespace crucible {

namespace {
juce::String pad(int n) { return juce::String(n).paddedLeft('0', 3); }
}

juce::String floatSlotId   (int i) { return "slot.f." + pad(i); }
juce::String boolSlotId    (int i) { return "slot.b." + pad(i); }
juce::String bargraphSlotId(int i) { return "slot.m." + pad(i); }

juce::AudioProcessorValueTreeState::ParameterLayout makeSlotPoolLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kFloatSlotCount; ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { floatSlotId(i), 1 },
            floatSlotId(i),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f),
            0.0f));

    for (int i = 0; i < kBoolSlotCount; ++i)
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { boolSlotId(i), 1 },
            boolSlotId(i),
            false));

    for (int i = 0; i < kBargraphSlotCount; ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bargraphSlotId(i), 1 },
            bargraphSlotId(i),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f),
            0.0f));

    return layout;
}

} // namespace crucible
