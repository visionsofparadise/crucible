#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include <Core/CrucibleCore.h>

namespace crucible { class WebviewBridge; }

/**
 * Crucible FX — VST3 effect variant. Audio in -> audio out, no MIDI.
 *
 * Phase 5 wires:
 *   - `crucible::FaustRuntime`  — compiles + hosts the Faust DSP
 *   - `crucible::SourceLoader`  — watched `.dsp` file path + buffer
 *   - `crucible::ParamBridge`   — APVTS slot pool ↔ Faust UI zones
 *
 * Phase 6 wires:
 *   - `crucible::MetersBridge`  — SPSC FIFO carrying IN/OUT level
 *                                  meter frames + bargraph snapshot to
 *                                  the UI thread (Phase 7's 30 Hz timer)
 *
 * The compile callback (`runtime.setCompileCallback`) fires on the
 * COMPILE WORKER THREAD; we wrap the APVTS-touching work in
 * `juce::MessageManager::callAsync` to reach message-thread context.
 *
 * Cross-load gating: FX accepts programs with at least one audio
 * input. Generator-shaped sources (zero inputs) are rejected by the
 * runtime's compile predicate; the rejection surfaces as a
 * `CompileResult` with `crossLoad = true` and a user-facing message.
 */
class CrucibleFXProcessor : public juce::AudioProcessor
{
public:
    CrucibleFXProcessor();
    ~CrucibleFXProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crucible FX"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override                     { return 1; }
    int  getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override              {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // Core runtime members — public so the (future) editor can plumb
    // them into the webview bridge without round-tripping through
    // accessors. Construction order matters: `apvts` first, then
    // `runtime`, then `loader`, then `bridge` (which references both).
    crucible::FaustRuntime runtime;
    crucible::SourceLoader loader;
    crucible::ParamBridge  bridge;
    crucible::MetersBridge meters;

    // Set by the editor on construction; cleared on editor
    // destruction. The message-thread callbacks (compile, source,
    // error) read this and, if non-null, push the event to the
    // webview. Atomic for the read in the callback path; only the
    // editor (message thread) writes.
    std::atomic<crucible::WebviewBridge*> webBridge { nullptr };

    // Last CompileResult forwarded to the webview, cached so a new
    // editor can replay the schema/error state on open. Touched only
    // on the message thread (compile-callback hop + replay listener).
    crucible::CompileResult cachedCompileResult;
    bool                    haveCachedCompileResult = false;

private:
    // Bargraph readback buffer of channel pointers — sized to the
    // max bus shape on `prepareToPlay` so `processBlock` never
    // allocates. The pointers themselves are set per-block.
    std::vector<float*>       processOutPtrs;
    std::vector<const float*> processInPtrs;
    juce::AudioBuffer<float>  silentInputBuffer; // used when no audio in

    // Kill switch for the compile callback. Captured by-value into the
    // worker-thread callback so the lambda outlives `*this`. Cleared in
    // the destructor so any in-flight callAsync lambdas (which capture
    // a shared_ptr copy) bail before touching freed members.
    std::shared_ptr<std::atomic<bool>> alive;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleFXProcessor)
};
