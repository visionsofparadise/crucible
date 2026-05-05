#include "PluginEditor.h"

#include "PluginProcessor.h"

CrucibleInstrumentEditor::CrucibleInstrumentEditor (CrucibleInstrumentProcessor& processor)
    : juce::AudioProcessorEditor (&processor)
{
    setSize (480, 320);
    setResizable (false, false);
}

CrucibleInstrumentEditor::~CrucibleInstrumentEditor() = default;

void CrucibleInstrumentEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (0x1C, 0x1A, 0x17));
    g.setColour (juce::Colour::fromRGB (0xE8, 0xE3, 0xDB));
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText ("Crucible Instrument", getLocalBounds(), juce::Justification::centred, 1);
}

void CrucibleInstrumentEditor::resized() {}
