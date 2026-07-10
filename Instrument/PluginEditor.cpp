#include "PluginEditor.h"

#include "PluginProcessor.h"

CrucibleInstrumentEditor::CrucibleInstrumentEditor (CrucibleInstrumentProcessor& processor)
    : juce::AudioProcessorEditor (&processor)
    , processorRef (processor)
{
    webBridge = std::make_unique<crucible::WebviewBridge> (
        processor.apvts,
        processor.runtime,
        processor.bridge,
        processor.loader,
        processor.meters,
        crucible::WebviewBridge::Variant::Instrument,
        [&processor]() -> const crucible::CompileResult*
        {
            return processor.haveCachedCompileResult ? &processor.cachedCompileResult
                                                     : nullptr;
        });

    addAndMakeVisible (*webBridge);

    // setSize must run AFTER the bridge is added — JUCE invokes
    // resized() from inside setSize, and resized() sizes the bridge to
    // fill the editor. If the bridge isn't a child yet, the inner
    // webview stays at 0×0 and renders as a black rectangle.
    setSize (680, 744);
    setResizable (false, false);

    processorRef.webBridge.store (webBridge.get(), std::memory_order_release);
}

CrucibleInstrumentEditor::~CrucibleInstrumentEditor()
{
    processorRef.webBridge.store (nullptr, std::memory_order_release);
}

void CrucibleInstrumentEditor::resized()
{
    if (webBridge != nullptr)
        webBridge->setBounds (getLocalBounds());
}
