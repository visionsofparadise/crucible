#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <Core/WebviewBridge.h>

class CrucibleInstrumentProcessor;

/**
 * Crucible Instrument editor — hosts a `crucible::WebviewBridge`
 * filling the fixed 920×760 window. See `CrucibleFXEditor` for the
 * shared bridge wiring rationale.
 */
class CrucibleInstrumentEditor : public juce::AudioProcessorEditor
{
public:
    explicit CrucibleInstrumentEditor (CrucibleInstrumentProcessor& processor);
    ~CrucibleInstrumentEditor() override;

    void resized() override;

private:
    CrucibleInstrumentProcessor& processorRef;

    std::unique_ptr<crucible::WebviewBridge> webBridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrucibleInstrumentEditor)
};
