#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class CrucibleFXProcessor;

/**
 * Crucible FX editor — placeholder. Will host a `WebBrowserComponent`
 * pointing at the shared `ui/` bundle in a later phase.
 */
class CrucibleFXEditor : public juce::AudioProcessorEditor
{
public:
    explicit CrucibleFXEditor (CrucibleFXProcessor& processor);
    ~CrucibleFXEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleFXEditor)
};
