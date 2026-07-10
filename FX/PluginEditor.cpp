#include "PluginEditor.h"

#include "PluginProcessor.h"

CrucibleFXEditor::CrucibleFXEditor (CrucibleFXProcessor& processor)
    : juce::AudioProcessorEditor (&processor)
    , processorRef (processor)
{
    webBridge = std::make_unique<crucible::WebviewBridge> (
        processor.apvts,
        processor.runtime,
        processor.bridge,
        processor.loader,
        processor.meters,
        crucible::WebviewBridge::Variant::FX,
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
    setSize (920, 760);
    setResizable (false, false);

    // Publish the bridge to the processor so its message-thread
    // compile / source / error callbacks can forward events to the
    // webview. Cleared in the destructor before the bridge is freed.
    processorRef.webBridge.store (webBridge.get(), std::memory_order_release);
}

CrucibleFXEditor::~CrucibleFXEditor()
{
    processorRef.webBridge.store (nullptr, std::memory_order_release);
}

void CrucibleFXEditor::resized()
{
    if (webBridge != nullptr)
        webBridge->setBounds (getLocalBounds());
}
