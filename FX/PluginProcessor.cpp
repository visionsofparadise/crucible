#include "PluginProcessor.h"

#include "PluginEditor.h"

CrucibleFXProcessor::CrucibleFXProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

CrucibleFXProcessor::~CrucibleFXProcessor() = default;

void CrucibleFXProcessor::prepareToPlay (double, int) {}
void CrucibleFXProcessor::releaseResources() {}

bool CrucibleFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn.isDisabled() || mainOut.isDisabled())
        return false;

    return mainIn == mainOut;
}

void CrucibleFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels not backed by an input (per JUCE convention).
    for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // Placeholder: passthrough.
}

juce::AudioProcessorEditor* CrucibleFXProcessor::createEditor()
{
    return new CrucibleFXEditor (*this);
}

void CrucibleFXProcessor::getStateInformation (juce::MemoryBlock&) {}
void CrucibleFXProcessor::setStateInformation (const void*, int) {}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrucibleFXProcessor();
}
