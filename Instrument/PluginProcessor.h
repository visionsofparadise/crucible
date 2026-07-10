#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include <Core/CrucibleCore.h>

namespace crucible { class WebviewBridge; }

/**
 * Crucible Instrument — VST3 instrument variant. MIDI in -> audio out.
 *
 * Phase 5 wires:
 *   - `crucible::FaustRuntime`  — compiles + hosts the Faust DSP
 *   - `crucible::SourceLoader`  — watched `.dsp` file path + buffer
 *   - `crucible::ParamBridge`   — APVTS slot pool ↔ Faust UI zones
 *
 * Phase 6 wires:
 *   - `crucible::MetersBridge`  — SPSC FIFO carrying OUT level meter
 *                                  frames + bargraph snapshot
 *   - `crucible::MidiBridge`    — libfaust MidiUI glue; processes the
 *                                  JUCE MidiBuffer per block BEFORE
 *                                  the runtime processes audio
 *
 * The compile callback fires on the COMPILE WORKER THREAD; we hop to
 * the message thread via `juce::MessageManager::callAsync` before
 * touching APVTS / ParamBridge.
 *
 * Cross-load gating: Instrument accepts generator-shaped programs
 * only (numInputs == 0). Effect-shaped sources are rejected by the
 * runtime's compile predicate; the rejection surfaces as a
 * `CompileResult` with `crossLoad = true`.
 */
class CrucibleInstrumentProcessor : public juce::AudioProcessor
{
public:
    CrucibleInstrumentProcessor();
    ~CrucibleInstrumentProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crucible Instrument"; }
    bool acceptsMidi() const override  { return true; }
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

    // MidiBridge is declared BEFORE FaustRuntime so it outlives the
    // runtime in destruction. The `mydsp_poly` DSP instance owned by
    // the runtime caches a back-pointer to the most-recently-bound
    // `MidiUI` (set by `mydsp_poly::buildUserInterface`'s
    // `dynamic_cast<midi_interface*>` branch). The poly's destructor
    // calls `fMidiHandler->removeMidiIn(this)` on that pointer, so the
    // MidiUI (owned by the bridge's `live`/`retired` lists) and the
    // bridge's underlying `midi_handler` must still be alive when the
    // poly is destroyed. Member destruction is reverse-declaration: by
    // putting MidiBridge first, FaustRuntime (and its poly) destruct
    // first, while the bridge — and the MidiUI the poly references —
    // is still live.
    crucible::MidiBridge   midiBridge;

    // Construction order: apvts → runtime → loader → bridge (depends on
    // both apvts and runtime).
    crucible::FaustRuntime runtime;
    crucible::SourceLoader loader;
    crucible::ParamBridge  bridge;
    crucible::MetersBridge meters;

    // Set by the editor on construction; cleared on editor
    // destruction. The message-thread callbacks (compile, source,
    // error) read this and, if non-null, push the event to the
    // webview.
    std::atomic<crucible::WebviewBridge*> webBridge { nullptr };

    // Last CompileResult forwarded to the webview, cached so a new
    // editor can replay the schema/error state on open. Touched only
    // on the message thread (compile-callback hop + replay listener).
    crucible::CompileResult cachedCompileResult;
    bool                    haveCachedCompileResult = false;

private:
    std::vector<float*>       processOutPtrs;
    std::vector<const float*> processInPtrs; // empty for generator-shaped programs

    // Kill switch for the compile callback — see CrucibleFXProcessor.
    std::shared_ptr<std::atomic<bool>> alive;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleInstrumentProcessor)
};
