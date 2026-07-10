// Phase 9 verification: SlotPool ID padding + layout shape.
//
// Why this test exists: the ID format spec is `paddedLeft('0', 3)` —
// a one-character off-by-one (or an accidental swap to paddedRight)
// silently produces wrong IDs that compile but break DAW automation
// persistence on every project reopen. APVTS lookups would miss and
// fall back to a fresh slot, scrambling user-saved automation lanes.
// Hard to spot by reading; cheap to assert here.

#include <catch2/catch_test_macros.hpp>

#include "SlotPool.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    // Walk the APVTS layout into a flat vector of raw pointers. The
    // layout's parameters are move-only, so we can't copy them out;
    // the easiest way to inspect counts/ranges is to attach the layout
    // to a stub processor and read parameters back through that.
    class StubProcessor : public juce::AudioProcessor
    {
    public:
        StubProcessor()
            : juce::AudioProcessor (BusesProperties())
            , apvts (*this, nullptr, "Test", crucible::makeSlotPoolLayout())
        {}
        const juce::String getName() const override { return "Stub"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override  { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int  getNumPrograms() override                     { return 1; }
        int  getCurrentProgram() override                  { return 0; }
        void setCurrentProgram (int) override              {}
        const juce::String getProgramName (int) override   { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };
}

TEST_CASE ("SlotPool IDs zero-pad to width 3", "[slot-pool]")
{
    // Mid-range — must be three digits, not two ("slot.f.12") and not
    // four ("slot.f.0012"). The current spec is exactly three.
    REQUIRE (crucible::floatSlotId    (12) == "slot.f.012");
    REQUIRE (crucible::boolSlotId     (12) == "slot.b.012");
    REQUIRE (crucible::bargraphSlotId (12) == "slot.m.012");

    // Single-digit values still pad to three.
    REQUIRE (crucible::floatSlotId    (3) == "slot.f.003");
    REQUIRE (crucible::boolSlotId     (3) == "slot.b.003");
    REQUIRE (crucible::bargraphSlotId (3) == "slot.m.003");
}

TEST_CASE ("SlotPool IDs at boundary indices", "[slot-pool]")
{
    // Index 0 — the bypass slot is slot.b.000 by name; a misformatted
    // pad on this value would silently break the bypass binding.
    REQUIRE (crucible::floatSlotId    (0) == "slot.f.000");
    REQUIRE (crucible::boolSlotId     (0) == "slot.b.000");
    REQUIRE (crucible::bargraphSlotId (0) == "slot.m.000");

    // Upper boundary of each pool — still three digits at 127 (floats),
    // 31 (bools), 15 (bargraphs). Catches a hypothetical width=2
    // regression that would only break at the top of the float pool.
    REQUIRE (crucible::floatSlotId    (crucible::kFloatSlotCount    - 1) == "slot.f.127");
    REQUIRE (crucible::boolSlotId     (crucible::kBoolSlotCount     - 1) == "slot.b.031");
    REQUIRE (crucible::bargraphSlotId (crucible::kBargraphSlotCount - 1) == "slot.m.015");
}

TEST_CASE ("SlotPool layout has the expected parameter counts and ranges", "[slot-pool]")
{
    StubProcessor proc;

    // Total count = 128 + 32 + 16 = 176. A wrong loop bound on any of
    // the three pools would change this and break APVTS layout
    // versioning silently.
    const auto& params = proc.getParameters();
    REQUIRE (params.size() == crucible::kFloatSlotCount
                            + crucible::kBoolSlotCount
                            + crucible::kBargraphSlotCount);

    // Every float slot exists, is an AudioParameterFloat over [0, 1]
    // defaulting to 0.
    for (int i = 0; i < crucible::kFloatSlotCount; ++i)
    {
        const auto id = crucible::floatSlotId (i);
        auto* p = proc.apvts.getParameter (id);
        REQUIRE (p != nullptr);

        auto* asFloat = dynamic_cast<juce::AudioParameterFloat*> (p);
        REQUIRE (asFloat != nullptr);
        REQUIRE (asFloat->getNormalisableRange().start == 0.0f);
        REQUIRE (asFloat->getNormalisableRange().end   == 1.0f);
        // Default is 0; the parameter's current value should equal the
        // default before any host writes.
        REQUIRE (static_cast<float> (*asFloat) == 0.0f);
    }

    // Every bool slot exists, is an AudioParameterBool defaulting to
    // false. slot.b.000 is reserved for bypass but the default-false
    // contract still holds.
    for (int i = 0; i < crucible::kBoolSlotCount; ++i)
    {
        const auto id = crucible::boolSlotId (i);
        auto* p = proc.apvts.getParameter (id);
        REQUIRE (p != nullptr);

        auto* asBool = dynamic_cast<juce::AudioParameterBool*> (p);
        REQUIRE (asBool != nullptr);
        REQUIRE (asBool->get() == false);
    }

    // Every bargraph slot exists, is an AudioParameterFloat over [0, 1]
    // defaulting to 0 (the bargraph readback contract).
    for (int i = 0; i < crucible::kBargraphSlotCount; ++i)
    {
        const auto id = crucible::bargraphSlotId (i);
        auto* p = proc.apvts.getParameter (id);
        REQUIRE (p != nullptr);

        auto* asFloat = dynamic_cast<juce::AudioParameterFloat*> (p);
        REQUIRE (asFloat != nullptr);
        REQUIRE (asFloat->getNormalisableRange().start == 0.0f);
        REQUIRE (asFloat->getNormalisableRange().end   == 1.0f);
        REQUIRE (static_cast<float> (*asFloat) == 0.0f);
    }
}
