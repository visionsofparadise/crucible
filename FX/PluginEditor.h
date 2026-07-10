#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <Core/WebviewBridge.h>

class CrucibleFXProcessor;

/**
 * Crucible FX editor — hosts a `crucible::WebviewBridge` filling the
 * fixed 920×760 window. The bridge owns the JUCE WebBrowserComponent,
 * the message channel, the 30 Hz meter/drain timer, the click-on-top
 * behaviour, and the external-link interception.
 */
class CrucibleFXEditor : public juce::AudioProcessorEditor
{
public:
    explicit CrucibleFXEditor (CrucibleFXProcessor& processor);
    ~CrucibleFXEditor() override;

    void resized() override;

private:
    CrucibleFXProcessor& processorRef;

    std::unique_ptr<crucible::WebviewBridge> webBridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleFXEditor)
};
