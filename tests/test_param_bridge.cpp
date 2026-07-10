// Phase 5.2 verification: ParamBridge round-trips APVTS slot writes
// to Faust zones, allocates fresh slots for new addresses across hot
// reloads, and rebinds persisted addresses to their original slots.
//
// We drive FaustRuntime directly (no SourceLoader, no message loop) so
// the test runs in the same single-threaded shape as test_faust_runtime.
// The compile callback fires on the worker thread; we serialise the
// rebindForSchema + assertion phase under a mutex + condition variable.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "FaustRuntime.h"
#include "ParamBridge.h"
#include "Schema.h"
#include "SlotPool.h"
#include "StdlibExtractor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace
{
    // Stub processor — APVTS needs an AudioProcessor to attach to. We
    // never call its audio methods; only APVTS construction matters.
    class StubProcessor : public juce::AudioProcessor
    {
    public:
        StubProcessor()
            : juce::AudioProcessor (BusesProperties()
                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
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

    struct CompileSignal
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::optional<crucible::CompileResult> latest;
        int count = 0;

        void onCompile (const crucible::CompileResult& r)
        {
            std::lock_guard<std::mutex> lock (mutex);
            latest = r;
            ++count;
            cv.notify_all();
        }

        bool waitForCount (int target, int timeoutMs)
        {
            std::unique_lock<std::mutex> lock (mutex);
            return cv.wait_for (lock, std::chrono::milliseconds (timeoutMs),
                                [this, target] { return count >= target; });
        }
    };

    std::vector<juce::File> defaultIncludes()
    {
        return { crucible::StdlibExtractor::ensureExtracted() };
    }

    // Compile a source and rebind the bridge. Returns the parsed schema
    // (owned by the caller).
    std::unique_ptr<crucible::ui::Schema>
    compileAndRebind (crucible::FaustRuntime& runtime,
                      crucible::ParamBridge&  bridge,
                      CompileSignal&          signal,
                      const std::string&      source)
    {
        const int before = [&]
        {
            std::lock_guard<std::mutex> lock (signal.mutex);
            return signal.count;
        }();

        runtime.submit (source);
        REQUIRE (signal.waitForCount (before + 1, 15000));
        REQUIRE (runtime.waitForIdle (5000));

        crucible::CompileResult result;
        {
            std::lock_guard<std::mutex> lock (signal.mutex);
            REQUIRE (signal.latest.has_value());
            REQUIRE (signal.latest->success);
            result = *signal.latest;
        }

        auto schema = crucible::ui::parseSchema (result.jsonUi);
        REQUIRE (schema != nullptr);
        bridge.rebindForSchema (*schema);
        return schema;
    }
}

TEST_CASE ("ParamBridge denormalises APVTS slot values into Faust zones", "[param-bridge]")
{
    StubProcessor proc;
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    crucible::ParamBridge bridge (proc.apvts, runtime);

    // Source A: one hslider "cutoff" in 20..20000 Hz.
    const std::string srcA =
        "import(\"stdfaust.lib\");"
        "cutoff = hslider(\"cutoff\", 1000, 20, 20000, 1);"
        "process = _ * cutoff / 20000.0;";

    auto schemaA = compileAndRebind (runtime, bridge, signal, srcA);
    REQUIRE (schemaA->flatControls.size() == 1);

    auto* cutoff = schemaA->flatControls.front();
    REQUIRE (cutoff->kind == crucible::ui::ControlKind::HSlider);
    REQUIRE (cutoff->min  == 20.0f);
    REQUIRE (cutoff->max  == 20000.0f);
    REQUIRE_FALSE (cutoff->slotId.empty());
    REQUIRE (cutoff->slotId.rfind ("slot.f.", 0) == 0); // starts with "slot.f."

    // Set the slot's normalised value to 0.5. setValueNotifyingHost
    // updates the parameter's raw atomic synchronously; the APVTS
    // listener notification path (which ParamBridge subscribes to)
    // is asynchronous and requires a running message-pump that this
    // headless Catch2 test cannot drive (see test_source_loader.cpp's
    // header comment on the one-shot stopDispatchLoop limitation).
    //
    // To verify the denormalisation contract end-to-end, we trigger
    // the same code path that ParamBridge's seed step uses on every
    // rebind: a re-rebind for the same schema reads the current APVTS
    // atomic and forwards via runtime.setZone — identical to the
    // listener path. The listener path is exercised in production
    // builds where a message-pump exists.
    auto* param = proc.apvts.getParameter (juce::String (cutoff->slotId));
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (0.5f);

    bridge.rebindForSchema (*schemaA);

    // 0.5 of [20..20000] is 10010 Hz.
    const float zone = runtime.getZone (cutoff->address);
    REQUIRE (zone == Catch::Approx (10010.0f).margin (1.0f));
}

TEST_CASE ("ParamBridge allocates a different slot for a new address", "[param-bridge]")
{
    StubProcessor proc;
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    crucible::ParamBridge bridge (proc.apvts, runtime);

    const std::string srcA =
        "import(\"stdfaust.lib\");"
        "cutoff = hslider(\"cutoff\", 1000, 20, 20000, 1);"
        "process = _ * cutoff / 20000.0;";

    auto schemaA = compileAndRebind (runtime, bridge, signal, srcA);
    REQUIRE (schemaA->flatControls.size() == 1);
    const auto slotA = schemaA->flatControls.front()->slotId;
    const auto addrA = schemaA->flatControls.front()->address;
    REQUIRE_FALSE (slotA.empty());

    // Submit a source that keeps `cutoff` AND adds a `resonance` slider.
    // The persistent address `cutoff` should keep its slot; the new
    // `resonance` should pick a different slot.
    const std::string srcB =
        "import(\"stdfaust.lib\");"
        "cutoff    = hslider(\"cutoff\", 1000, 20, 20000, 1);"
        "resonance = hslider(\"resonance\", 0.5, 0, 1, 0.01);"
        "process = _ * cutoff / 20000.0 * resonance;";

    auto schemaB = compileAndRebind (runtime, bridge, signal, srcB);
    REQUIRE (schemaB->flatControls.size() == 2);

    std::string cutoffSlotB, resonanceSlotB;
    for (auto* c : schemaB->flatControls)
    {
        if (c->address == addrA) cutoffSlotB = c->slotId;
        else                     resonanceSlotB = c->slotId;
    }
    REQUIRE (cutoffSlotB == slotA);       // persisted
    REQUIRE_FALSE (resonanceSlotB.empty());
    REQUIRE (resonanceSlotB != slotA);    // distinct from existing binding
}

TEST_CASE ("ParamBridge restores persisted address-to-slot bindings", "[param-bridge]")
{
    StubProcessor proc;
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    juce::ValueTree savedMapping;
    std::string     originalSlotId;
    std::string     originalAddress;

    {
        crucible::ParamBridge bridge (proc.apvts, runtime);
        const std::string srcA =
            "import(\"stdfaust.lib\");"
            "freq = hslider(\"freq\", 440, 20, 20000, 1);"
            "process = os.osc(freq);";
        auto schemaA = compileAndRebind (runtime, bridge, signal, srcA);
        REQUIRE (schemaA->flatControls.size() == 1);
        originalSlotId  = schemaA->flatControls.front()->slotId;
        originalAddress = schemaA->flatControls.front()->address;
        REQUIRE_FALSE (originalSlotId.empty());

        savedMapping = bridge.saveMapping();
    }

    // Fresh bridge, restore the mapping, then rebind on the same source.
    // The address must land on its original slot, not a freshly allocated
    // one.
    crucible::ParamBridge bridge (proc.apvts, runtime);
    bridge.restoreMapping (savedMapping);

    const std::string srcA =
        "import(\"stdfaust.lib\");"
        "freq = hslider(\"freq\", 440, 20, 20000, 1);"
        "process = os.osc(freq);";
    auto schemaA = compileAndRebind (runtime, bridge, signal, srcA);
    REQUIRE (schemaA->flatControls.size() == 1);
    REQUIRE (schemaA->flatControls.front()->address == originalAddress);
    REQUIRE (schemaA->flatControls.front()->slotId  == originalSlotId);
}

TEST_CASE ("ParamBridge readBargraphsIntoSlots mirrors zones into APVTS atomics",
           "[param-bridge]")
{
    StubProcessor proc;
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    crucible::ParamBridge bridge (proc.apvts, runtime);

    // A passthrough with one hbargraph; we manually push a value into
    // the Faust zone via setZone, then readBargraphsIntoSlots and check
    // the normalised result landed in the apvts atomic.
    const std::string src =
        "import(\"stdfaust.lib\");"
        "level = hbargraph(\"level\", 0, 1);"
        "process = _ <: attach(_, level);";

    auto schema = compileAndRebind (runtime, bridge, signal, src);
    REQUIRE (schema->flatControls.size() == 1);
    auto* bg = schema->flatControls.front();
    REQUIRE (crucible::ui::isBargraphKind (bg->kind));
    REQUIRE_FALSE (bg->slotId.empty());
    REQUIRE (bg->slotId.rfind ("slot.m.", 0) == 0);

    // Stuff a value into the zone directly; this would normally happen
    // mid-block from Faust's compute. 0.75 in 0..1 → 0.75 normalised.
    runtime.setZone (bg->address, 0.75f);
    bridge.readBargraphsIntoSlots();

    auto* atom = proc.apvts.getRawParameterValue (juce::String (bg->slotId));
    REQUIRE (atom != nullptr);
    REQUIRE (atom->load() == Catch::Approx (0.75f).margin (1e-6f));
}
