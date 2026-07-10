#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <Core/WebviewBridge.h>

#include <cmath>

namespace
{
    // Upper bound on Faust DSP channel counts we pre-size the audio-
    // thread pointer array to. 32 covers surround formats well past
    // 7.1.4 at a cost of 256 bytes.
    constexpr int kMaxDspChannels = 32;

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

CrucibleInstrumentProcessor::CrucibleInstrumentProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "Crucible", crucible::makeSlotPoolLayout())
    , midiBridge()
    , runtime()
    , loader()
    , bridge (apvts, runtime)
    , meters()
    , alive (std::make_shared<std::atomic<bool>> (true))
{
    // Enable polyphonic wrapping (16 voices) so the Faust runtime wraps
    // freshly-compiled instances in `mydsp_poly`. This is what makes
    // libfaust's MidiUI auto-route NoteOn / NoteOff to the voice-internal
    // `freq` / `gain` / `gate` zones — without the wrap, the plain
    // llvm_dsp does not implement the `midi` interface and notes are
    // silently dropped. FX does NOT call this: its monophonic effect-
    // shaped programs are correct as plain `llvm_dsp`.
    runtime.setPolyphony (16);

    runtime.setIncludePaths ({ crucible::StdlibExtractor::ensureExtracted() });

    // Cross-load gate: Instrument accepts generator-shaped programs
    // only (zero audio inputs).
    runtime.setCompilePredicate ([] (const crucible::CompileResult& r)
    {
        return r.numInputs == 0;
    });

    // Compile callback fires on the worker thread; hop to message and
    // kill-switch-guard against destruction (see CrucibleFXProcessor).
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

                // Phase 6: bind libfaust's MidiUI to the freshly-
                // installed DSP so its `[midi:...]` mappings declared
                // in the Faust source are picked up. The runtime's
                // worker has already performed the atomic swap, so
                // `getCurrentDsp` / `getCurrentMidi` return the new
                // instance and its (poly-only) MIDI interface.
                midiBridge.bind (runtime.getCurrentDsp(), runtime.getCurrentMidi());

                setLatencySamples (result.latencySamples);
                runtime.drainDeletions();
                midiBridge.drainRetired();
            }

            // Cache the result so a newly-opened editor can replay
            // schema/error state on mount. Always update, regardless of
            // success.
            cachedCompileResult     = result;
            haveCachedCompileResult = true;

            if (auto* wb = webBridge.load (std::memory_order_acquire))
                wb->onCompileResult (result, schema.get());
        });
    });

    loader.setCallbacks (
        [this] (const juce::String& source, const juce::File& path)
        {
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
            if (auto* wb = webBridge.load (std::memory_order_acquire))
                wb->onInputError (error);
        });
}

CrucibleInstrumentProcessor::~CrucibleInstrumentProcessor()
{
    // Stop the compile callback from racing with destruction.
    // See CrucibleFXProcessor::~CrucibleFXProcessor for the rationale.
    alive->store (false, std::memory_order_release);
    runtime.setCompileCallback ({});
    // Drop the audio-thread-visible Bound pointer so any in-flight
    // `processBlock` call sees null and bails out. The retired MidiUI
    // and the poly DSP must NOT be destroyed here — the poly's
    // destructor (run when `~runtime` fires as part of member
    // destruction below) dereferences the MidiUI's `midi_handler` to
    // remove itself. Member-destruction order is FaustRuntime → ...
    // → MidiBridge (see PluginProcessor.h's member-order comment),
    // which keeps the MidiUI alive until after the poly is gone.
    midiBridge.bind (nullptr, nullptr);
}

void CrucibleInstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    runtime.setSampleRate (sampleRate);
    // Pre-size so `processBlock` never resizes on the audio thread.
    processOutPtrs.assign (
        static_cast<size_t> (juce::jmax (getMainBusNumOutputChannels(), kMaxDspChannels)), nullptr);
}

void CrucibleInstrumentProcessor::releaseResources()
{
    runtime.drainDeletions();
}

bool CrucibleInstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut.isDisabled())
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void CrucibleInstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numFrames        = buffer.getNumSamples();
    const int totalOutChannels = getTotalNumOutputChannels();

    crucible::MeterFrame frame; // IN remains zero — Instrument has no audio input.

    auto pushMeterFrame = [&]
    {
        const auto leftLvl  = measureChannel (
            totalOutChannels > 0 ? buffer.getReadPointer (0) : nullptr, numFrames);
        const float* rOut = (totalOutChannels >= 2)
            ? buffer.getReadPointer (1)
            : (totalOutChannels == 1 ? buffer.getReadPointer (0) : nullptr);
        const auto rightLvl = measureChannel (rOut, numFrames);
        frame.outL_rms  = leftLvl.rms;   frame.outL_peak = leftLvl.peak;
        frame.outR_rms  = rightLvl.rms;  frame.outR_peak = rightLvl.peak;
        bridge.snapshotBargraphsInto (frame.bargraphValues);
        meters.tryPush (frame);
    };

    const bool bypassed = apvts.getRawParameterValue (
        crucible::boolSlotId (crucible::kBypassSlotIndex))->load() > 0.5f;
    if (bypassed)
    {
        // Instrument bypass = silence (no input audio to pass through).
        buffer.clear();
        pushMeterFrame();
        return;
    }

    // Dispatch incoming MIDI into the libfaust MidiUI BEFORE the runtime
    // processes audio so the new zone values (driven by CC / keyOn /
    // etc.) take effect within this block.
    midiBridge.process (midiBuffer);

    const int dspOuts = runtime.currentNumOutputs();

    if (dspOuts == 0)
    {
        // No source loaded — emit silence (Instrument default).
        buffer.clear();
        bridge.readBargraphsIntoSlots();
        pushMeterFrame();
        return;
    }

    if ((int) processOutPtrs.size() < dspOuts)
        processOutPtrs.resize (static_cast<size_t> (dspOuts), nullptr);

    for (int c = 0; c < dspOuts; ++c)
        processOutPtrs[(size_t) c] =
            (c < totalOutChannels) ? buffer.getWritePointer (c) : nullptr;

    // Generator-shaped Faust programs have 0 inputs. Pass a null
    // input-pointer array — `runtime.process` invokes dsp->compute with
    // it as-is; libfaust ignores it when numInputs == 0.
    runtime.process (numFrames, nullptr, processOutPtrs.data());

    // If the host gave us more output channels than Faust filled
    // (mono Faust into a stereo bus), duplicate channel 0 to channel 1.
    if (dspOuts == 1 && totalOutChannels >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numFrames);

    bridge.readBargraphsIntoSlots();
    pushMeterFrame();
}

juce::AudioProcessorEditor* CrucibleInstrumentProcessor::createEditor()
{
    return new CrucibleInstrumentEditor (*this);
}

void CrucibleInstrumentProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("CrucibleInstrument");
    root.appendChild (apvts.copyState(),      nullptr);
    juce::ValueTree pathNode ("WatchedPath");
    pathNode.setProperty ("path", loader.currentPath().getFullPathName(), nullptr);
    root.appendChild (pathNode,               nullptr);
    root.appendChild (bridge.saveMapping(),   nullptr);

    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void CrucibleInstrumentProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("CrucibleInstrument"))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        return;
    }

    const auto apvtsState = root.getChildWithName (apvts.state.getType());
    if (apvtsState.isValid())
        apvts.replaceState (apvtsState);

    const auto mapping = root.getChildWithName ("ParamBridge");
    if (mapping.isValid())
        bridge.restoreMapping (mapping);

    const auto pathNode = root.getChildWithName ("WatchedPath");
    if (pathNode.isValid())
    {
        const juce::File path (pathNode.getProperty ("path").toString());
        if (path != juce::File())
            loader.restorePath (path);
    }
}

juce::AudioProcessorParameter* CrucibleInstrumentProcessor::getBypassParameter() const
{
    return apvts.getParameter (crucible::boolSlotId (crucible::kBypassSlotIndex));
}

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrucibleInstrumentProcessor();
}
