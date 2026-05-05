#include "PluginProcessor.h"

#include "PluginEditor.h"

CrucibleInstrumentProcessor::CrucibleInstrumentProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

CrucibleInstrumentProcessor::~CrucibleInstrumentProcessor() = default;

void CrucibleInstrumentProcessor::prepareToPlay (double, int) {}
void CrucibleInstrumentProcessor::releaseResources() {}

bool CrucibleInstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut.isDisabled())
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void CrucibleInstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Placeholder: produce silence. Future Core synthesis will fill the
    // output buffer here using MIDI events from the second argument.
    buffer.clear();
}

juce::AudioProcessorEditor* CrucibleInstrumentProcessor::createEditor()
{
    return new CrucibleInstrumentEditor (*this);
}

void CrucibleInstrumentProcessor::getStateInformation (juce::MemoryBlock&) {}
void CrucibleInstrumentProcessor::setStateInformation (const void*, int) {}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrucibleInstrumentProcessor();
}
