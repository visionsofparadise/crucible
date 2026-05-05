#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * Crucible Instrument — VST3 instrument variant. MIDI in -> audio out.
 *
 * Placeholder implementation: produces silence. Real synthesis will be
 * delegated to `crucible::` Core in a later phase.
 */
class CrucibleInstrumentProcessor : public juce::AudioProcessor
{
public:
    CrucibleInstrumentProcessor();
    ~CrucibleInstrumentProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

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

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleInstrumentProcessor)
};
