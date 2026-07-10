#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <Core/WebviewBridge.h>

#include <cmath>

namespace
{
    // Upper bound on Faust DSP channel counts we pre-size the audio-
    // thread pointer arrays to. Faust sources beyond this would force
    // a per-block `resize()`; with 32 we cover surround formats well
    // past 7.1.4 at a cost of 256 bytes per pointer array.
    constexpr int kMaxDspChannels = 32;

    // Compute RMS and peak over `numFrames` samples of one channel.
    struct ChannelLevel { float rms = 0.0f; float peak = 0.0f; };

    ChannelLevel measureChannel (const float* data, int numFrames) noexcept
    {
        ChannelLevel out;
        if (data == nullptr || numFrames <= 0)
            return out;
        float sumSquares = 0.0f;
        float peak       = 0.0f;
        for (int i = 0; i < numFrames; ++i)
        {
            const float v = data[i];
            sumSquares += v * v;
            const float a = std::fabs (v);
            if (a > peak)
                peak = a;
        }
        out.rms  = std::sqrt (sumSquares / (float) numFrames);
        out.peak = peak;
        return out;
    }
}

CrucibleFXProcessor::CrucibleFXProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "Crucible", crucible::makeSlotPoolLayout())
    , runtime()
    , loader()
    , bridge (apvts, runtime)
    , meters()
    , alive (std::make_shared<std::atomic<bool>> (true))
{
    // Include the bundled Faust stdlib on every compile. Extra paths
    // (the watched file's parent) are appended in setStateInformation
    // once a watched path is known.
    runtime.setIncludePaths ({ crucible::StdlibExtractor::ensureExtracted() });

    // Cross-load gate: FX accepts effect-shaped programs only
    // (>= 1 audio input). Generator-shaped sources are rejected before
    // the swap.
    runtime.setCompilePredicate ([] (const crucible::CompileResult& r)
    {
        return r.numInputs > 0;
    });

    // Compile callback fires on the worker thread. Hop to the message
    // thread before touching APVTS / ParamBridge / latency. The
    // `killSwitch` is a shared_ptr copy that outlives `*this`; the
    // destructor sets `*killSwitch = false` so any callAsync lambda
    // queued after destruction bails before dereferencing `this`.
    auto killSwitch = alive;
    runtime.setCompileCallback ([this, killSwitch] (const crucible::CompileResult& result)
    {
        juce::MessageManager::callAsync ([this, killSwitch, result]
        {
            if (! killSwitch->load (std::memory_order_acquire))
                return; // Processor destroyed; do not touch `this`.

            std::unique_ptr<crucible::ui::Schema> schema;
            if (result.success)
            {
                std::string err;
                schema = crucible::ui::parseSchema (result.jsonUi, &err);
                if (schema != nullptr)
                    bridge.rebindForSchema (*schema);

                setLatencySamples (result.latencySamples);
                runtime.drainDeletions();
            }

            // Cache the result so a newly-opened editor can replay
            // schema/error state on mount. Always update, regardless of
            // success — failed compiles should also be replayable so
            // the error overlay survives editor close/reopen.
            cachedCompileResult     = result;
            haveCachedCompileResult = true;

            // Forward to the webview if an editor is currently open.
            // The bridge runs on the message thread (we're in a
            // callAsync hop here), so direct invocation is safe.
            if (auto* wb = webBridge.load (std::memory_order_acquire))
                wb->onCompileResult (result, schema.get());
        });
    });

    // Source watcher → compile pipeline. The watcher fires on the
    // message thread; `runtime.submit` coalesces.
    loader.setCallbacks (
        [this] (const juce::String& source, const juce::File& path)
        {
            // Include the watched file's parent so user
            // `import("./helper.lib")` resolves relative to the source.
            runtime.setIncludePaths ({
                crucible::StdlibExtractor::ensureExtracted(),
                path.getParentDirectory()
            });
            runtime.submit (source.toStdString());

            if (auto* wb = webBridge.load (std::memory_order_acquire))
                wb->onSourceChanged (source, path);
        },
        [this] (const crucible::SourceLoader::InputError& error)
        {
            // File-missing / read-failure: last-known-good DSP keeps
            // running (per design). Surface to the webview error
            // overlay when the editor is open.
            if (auto* wb = webBridge.load (std::memory_order_acquire))
                wb->onInputError (error);
        });
}

CrucibleFXProcessor::~CrucibleFXProcessor()
{
    // Stop the compile callback from racing with destruction.
    // Step 1: flip the kill switch so any callAsync lambda already queued
    // bails before touching freed members.
    alive->store (false, std::memory_order_release);
    // Step 2: clear the runtime's compile callback so no NEW callbacks
    // are posted by the worker. (A callback fired between these two
    // statements still sees the kill switch and bails.)
    runtime.setCompileCallback ({});
}

void CrucibleFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    runtime.setSampleRate (sampleRate);
    silentInputBuffer.setSize (1, samplesPerBlock, false, true, true);
    silentInputBuffer.clear();
    // Pre-size to cover both the host bus shape AND any plausible Faust
    // DSP channel count, so `processBlock` never resizes on the audio
    // thread.
    processOutPtrs.assign (
        static_cast<size_t> (juce::jmax (getMainBusNumOutputChannels(), kMaxDspChannels)), nullptr);
    processInPtrs.assign (
        static_cast<size_t> (juce::jmax (getMainBusNumInputChannels(), kMaxDspChannels)), nullptr);
}

void CrucibleFXProcessor::releaseResources()
{
    runtime.drainDeletions();
}

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
    const int numFrames              = buffer.getNumSamples();

    // Clear any output channels not backed by an input (per JUCE convention).
    for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // === IN meter (computed BEFORE processing, per channel-handling rule) ===
    crucible::MeterFrame frame;
    {
        const auto leftLvl  = measureChannel (
            totalNumInputChannels > 0 ? buffer.getReadPointer (0) : nullptr, numFrames);
        // Mono -> duplicate ch 0 to R; 2ch -> ch 1 to R; >2 -> ch 1 to R (rest ignored).
        const float* rIn = (totalNumInputChannels >= 2)
            ? buffer.getReadPointer (1)
            : (totalNumInputChannels == 1 ? buffer.getReadPointer (0) : nullptr);
        const auto rightLvl = measureChannel (rIn, numFrames);
        frame.inL_rms  = leftLvl.rms;   frame.inL_peak = leftLvl.peak;
        frame.inR_rms  = rightLvl.rms;  frame.inR_peak = rightLvl.peak;
    }

    const bool bypassed = apvts.getRawParameterValue (
        crucible::boolSlotId (crucible::kBypassSlotIndex))->load() > 0.5f;

    auto pushMeterFrame = [&]
    {
        // OUT meter (post-processing).
        const auto leftLvl  = measureChannel (
            totalNumOutputChannels > 0 ? buffer.getReadPointer (0) : nullptr, numFrames);
        const float* rOut = (totalNumOutputChannels >= 2)
            ? buffer.getReadPointer (1)
            : (totalNumOutputChannels == 1 ? buffer.getReadPointer (0) : nullptr);
        const auto rightLvl = measureChannel (rOut, numFrames);
        frame.outL_rms  = leftLvl.rms;   frame.outL_peak = leftLvl.peak;
        frame.outR_rms  = rightLvl.rms;  frame.outR_peak = rightLvl.peak;
        bridge.snapshotBargraphsInto (frame.bargraphValues);
        meters.tryPush (frame);
    };

    if (bypassed)
    {
        // FX bypass = passthrough; leave the audio buffer unchanged.
        pushMeterFrame();
        return;
    }

    const int dspIns    = runtime.currentNumInputs();
    const int dspOuts   = runtime.currentNumOutputs();

    if (dspIns == 0 && dspOuts == 0)
    {
        // No source loaded — passthrough.
        bridge.readBargraphsIntoSlots();
        pushMeterFrame();
        return;
    }

    // Build input pointer array. If Faust expects more inputs than the
    // bus exposes (rare — `isBusesLayoutSupported` clamps this), fill
    // with silent input. If fewer, ignore extra bus channels.
    if ((int) processInPtrs.size() < dspIns)
        processInPtrs.resize (static_cast<size_t> (dspIns), nullptr);
    if ((int) processOutPtrs.size() < dspOuts)
        processOutPtrs.resize (static_cast<size_t> (dspOuts), nullptr);

    for (int c = 0; c < dspIns; ++c)
    {
        processInPtrs[(size_t) c] =
            (c < totalNumInputChannels)
                ? buffer.getReadPointer (c)
                : silentInputBuffer.getReadPointer (0);
    }
    for (int c = 0; c < dspOuts; ++c)
    {
        processOutPtrs[(size_t) c] =
            (c < totalNumOutputChannels)
                ? buffer.getWritePointer (c)
                : nullptr; // dropped channel
    }

    // In-place: Faust outputs may overwrite inputs since pointer arrays
    // are independent and JUCE buffers' per-channel arrays don't alias.
    runtime.process (numFrames, processInPtrs.data(), processOutPtrs.data());

    bridge.readBargraphsIntoSlots();
    pushMeterFrame();
}

juce::AudioProcessorEditor* CrucibleFXProcessor::createEditor()
{
    return new CrucibleFXEditor (*this);
}

void CrucibleFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Combine APVTS state + watched-file path + bridge mapping into a
    // single root tree. Reading back: restore APVTS first (so listeners
    // fire with stable values), then mapping (so subsequent rebinds
    // honour persisted bindings), then restorePath (so a synchronous
    // compile submission lands AFTER both of the above).
    juce::ValueTree root ("CrucibleFX");
    root.appendChild (apvts.copyState(),       nullptr);
    juce::ValueTree pathNode ("WatchedPath");
    pathNode.setProperty ("path", loader.currentPath().getFullPathName(), nullptr);
    root.appendChild (pathNode,                 nullptr);
    root.appendChild (bridge.saveMapping(),    nullptr);

    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void CrucibleFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("CrucibleFX"))
    {
        // Pre-Phase-5 state (raw APVTS XML): restore as before.
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return;
    }

    // APVTS state.
    const auto apvtsState = root.getChildWithName (apvts.state.getType());
    if (apvtsState.isValid())
        apvts.replaceState (apvtsState);

    // Bridge mapping (restore BEFORE the compile so the rebind keeps
    // persisted bindings).
    const auto mapping = root.getChildWithName ("ParamBridge");
    if (mapping.isValid())
        bridge.restoreMapping (mapping);

    // Watched path → restorePath fires synchronously and triggers the
    // compile pipeline. Members are fully constructed at this point.
    const auto pathNode = root.getChildWithName ("WatchedPath");
    if (pathNode.isValid())
    {
        const juce::File path (pathNode.getProperty ("path").toString());
        if (path != juce::File())
            loader.restorePath (path);
    }
}

juce::AudioProcessorParameter* CrucibleFXProcessor::getBypassParameter() const
{
    return apvts.getParameter (crucible::boolSlotId (crucible::kBypassSlotIndex));
}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrucibleFXProcessor();
}
