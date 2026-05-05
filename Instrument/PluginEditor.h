#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class CrucibleInstrumentProcessor;

/**
 * Crucible Instrument editor — placeholder. Will host a
 * `WebBrowserComponent` pointing at the shared `ui/` bundle in a later
 * phase.
 */
class CrucibleInstrumentEditor : public juce::AudioProcessorEditor
{
public:
    explicit CrucibleInstrumentEditor (CrucibleInstrumentProcessor& processor);
    ~CrucibleInstrumentEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleInstrumentEditor)
};
