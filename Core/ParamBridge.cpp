#include "ParamBridge.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace crucible {

namespace
{
    // Slot categories — float (hslider/vslider/nentry), bool
    // (button/checkbox), bargraph (hbargraph/vbargraph). Each control
    // address binds to exactly one slot in exactly one category.
    enum class SlotCategory { Float, Bool, Bargraph };

    SlotCategory categoryFor (ui::ControlKind k)
    {
        if (ui::isBargraphKind (k)) return SlotCategory::Bargraph;
        if (ui::isBoolKind     (k)) return SlotCategory::Bool;
        return SlotCategory::Float;
    }

    juce::String slotIdFor (SlotCategory cat, int index)
    {
        switch (cat)
        {
            case SlotCategory::Float:    return floatSlotId (index);
            case SlotCategory::Bool:     return boolSlotId (index);
            case SlotCategory::Bargraph: return bargraphSlotId (index);
        }
        return {};
    }

    int slotCountFor (SlotCategory cat)
    {
        switch (cat)
        {
            case SlotCategory::Float:    return kFloatSlotCount;
            case SlotCategory::Bool:     return kBoolSlotCount;
            case SlotCategory::Bargraph: return kBargraphSlotCount;
        }
        return 0;
    }

    // Parse a slot ID like "slot.f.012" into (category, index). Returns
    // false on unrecognised IDs (e.g. legacy bypass or future formats).
    bool parseSlotId (const juce::String& id, SlotCategory& cat, int& index)
    {
        if (! id.startsWith ("slot."))
            return false;
        if (id.length() < 9)
            return false;

        const auto kindChar = id[5];
        if      (kindChar == 'f') cat = SlotCategory::Float;
        else if (kindChar == 'b') cat = SlotCategory::Bool;
        else if (kindChar == 'm') cat = SlotCategory::Bargraph;
        else return false;

        index = id.substring (7).getIntValue();
        return index >= 0 && index < slotCountFor (cat);
    }

    // Snapshot of one bargraph binding consumed on the audio thread.
    // `zonePtr` is the raw Faust zone pointer (owned by the runtime's
    // currently-installed MapUI). Cached at rebind time via
    // `runtime.getZonePointer` so the audio thread can dereference it
    // without taking the runtime mutex. Lifetime is guaranteed by the
    // rebind-on-compile contract — see FaustRuntime::getZonePointer.
    // `paramAtom` is the APVTS raw atomic pointer for the bargraph
    // slot, stored normalised (0..1).
    struct BargraphBinding
    {
        std::atomic<float>* paramAtom = nullptr;
        float*              zonePtr   = nullptr;
        float               min       = 0.0f;
        float               range     = 1.0f; // max - min
    };

    float normalise (float value, float min, float max)
    {
        if (max <= min)
            return 0.0f;
        const float n = (value - min) / (max - min);
        return std::clamp (n, 0.0f, 1.0f);
    }

    float denormalise (float n, float min, float max)
    {
        return min + n * (max - min);
    }
}

struct ParamBridge::Impl
{
    Impl (juce::AudioProcessorValueTreeState& apvtsIn, FaustRuntime& runtimeIn)
        : apvts (apvtsIn), runtime (runtimeIn)
    {
    }

    juce::AudioProcessorValueTreeState& apvts;
    FaustRuntime&                       runtime;

    // Map: Faust address → (slot category, slot index). Persisted via
    // saveMapping / restoreMapping. Protected by `mutex` for
    // rebind/parameterChanged contention with the message-thread
    // listener callbacks.
    struct Binding
    {
        SlotCategory cat   = SlotCategory::Float;
        int          index = 0;
        // Cached denormalisation metadata, set on rebind. Float and
        // bargraph categories use min/max; bool ignores them.
        float        min   = 0.0f;
        float        max   = 1.0f;
    };

    mutable std::mutex                       mutex;
    std::unordered_map<std::string, Binding> addressToBinding; // by address

    // Reverse map: slot ID → bound address. Populated under `mutex`;
    // read on the message thread from `parameterChanged`.
    std::unordered_map<std::string, std::string> slotIdToAddress;

    // Audio-thread bargraph snapshot. Updated on rebind under `mutex`;
    // copied into `bargraphSnapshot` atomically (pointer-swap-style).
    // `readBargraphsIntoSlots` uses `bargraphSnapshotPtr.load()` and
    // iterates; no allocations.
    std::vector<BargraphBinding>             bargraphLatest;

    // Atomic pointer to the bargraphLatest snapshot — `readBargraphsIntoSlots`
    // dereferences this; rebindForSchema rebuilds the vector under the
    // mutex and then `store`s the new pointer. The vector itself is
    // stable for the lifetime of the bridge; we just publish a new view.
    std::atomic<const std::vector<BargraphBinding>*> bargraphView { nullptr };

    // We hold a stable storage cell for `bargraphLatest` so the
    // atomic pointer remains valid; double-buffered to avoid the
    // audio thread reading torn data mid-rebuild. `back` is the buffer
    // currently being rebuilt; `front` is what `bargraphView` points
    // at.
    std::vector<BargraphBinding>             bargraphBufferA;
    std::vector<BargraphBinding>             bargraphBufferB;
    bool                                     useBufferA = true;

    // Cached APVTS raw-atomic pointers for the bargraph readback slots.
    // Filled once at construction (apvts owns these for the lifetime of
    // the processor). Read by `snapshotBargraphsInto` from the audio
    // thread without locking — the array is stable, only the atomics'
    // values change.
    std::array<std::atomic<float>*, kBargraphSlotCount> bargraphSlotAtoms {};

    // Track which slot indices are taken per category. A boolean
    // vector is fine — slot counts are small (<= 128 each).
    std::vector<bool>                        floatTaken    { std::vector<bool> (kFloatSlotCount,    false) };
    std::vector<bool>                        boolTaken     { std::vector<bool> (kBoolSlotCount,     false) };
    std::vector<bool>                        bargraphTaken { std::vector<bool> (kBargraphSlotCount, false) };

    // Bypass slot (bool 0) is permanently reserved — never auto-allocated
    // to a Faust address. Apply once at construction.
    void reserveBypassSlot()
    {
        if (kBypassSlotIndex >= 0 && kBypassSlotIndex < kBoolSlotCount)
            boolTaken[(size_t) kBypassSlotIndex] = true;
    }

    bool isReserved (SlotCategory cat, int idx) const
    {
        return cat == SlotCategory::Bool && idx == kBypassSlotIndex;
    }

    int allocateSlot (SlotCategory cat)
    {
        auto& taken = (cat == SlotCategory::Float)    ? floatTaken
                    : (cat == SlotCategory::Bool)     ? boolTaken
                    :                                   bargraphTaken;
        const int n = (int) taken.size();
        for (int i = 0; i < n; ++i)
        {
            if (! taken[(size_t) i])
            {
                taken[(size_t) i] = true;
                return i;
            }
        }
        return -1;
    }

    void releaseSlot (SlotCategory cat, int idx)
    {
        auto& taken = (cat == SlotCategory::Float)    ? floatTaken
                    : (cat == SlotCategory::Bool)     ? boolTaken
                    :                                   bargraphTaken;
        if (idx >= 0 && idx < (int) taken.size())
            taken[(size_t) idx] = false;
    }

    void markTaken (SlotCategory cat, int idx)
    {
        auto& taken = (cat == SlotCategory::Float)    ? floatTaken
                    : (cat == SlotCategory::Bool)     ? boolTaken
                    :                                   bargraphTaken;
        if (idx >= 0 && idx < (int) taken.size())
            taken[(size_t) idx] = true;
    }
};

ParamBridge::ParamBridge (juce::AudioProcessorValueTreeState& apvts, FaustRuntime& runtime)
    : impl (std::make_unique<Impl> (apvts, runtime))
{
    impl->reserveBypassSlot();

    // Cache the bargraph readback slot atomics once. APVTS owns the
    // atomic for the lifetime of the processor, so these pointers are
    // valid for the bridge's lifetime.
    for (int i = 0; i < kBargraphSlotCount; ++i)
        impl->bargraphSlotAtoms[(size_t) i] = apvts.getRawParameterValue (bargraphSlotId (i));

    // Register as listener for every non-bypass float and bool slot.
    // Bargraphs are write-only from the bridge; no listener needed.
    for (int i = 0; i < kFloatSlotCount; ++i)
        apvts.addParameterListener (floatSlotId (i), this);
    for (int i = 0; i < kBoolSlotCount; ++i)
    {
        if (i == kBypassSlotIndex)
            continue; // Bypass is the processor's concern.
        apvts.addParameterListener (boolSlotId (i), this);
    }
}

ParamBridge::~ParamBridge()
{
    for (int i = 0; i < kFloatSlotCount; ++i)
        impl->apvts.removeParameterListener (floatSlotId (i), this);
    for (int i = 0; i < kBoolSlotCount; ++i)
    {
        if (i == kBypassSlotIndex)
            continue;
        impl->apvts.removeParameterListener (boolSlotId (i), this);
    }
}

void ParamBridge::rebindForSchema (ui::Schema& schema)
{
    // Init-value writes for newly-bound slots. Deferred outside the
    // lock so `setValueNotifyingHost` can fire listeners — including
    // our own `parameterChanged`, which acquires `impl->mutex` — without
    // deadlocking.
    struct PendingInit { juce::String slotId; float normalised; };
    std::vector<PendingInit> pendingInits;

    {
    std::lock_guard<std::mutex> lock (impl->mutex);

    // Step 1: drop any existing binding whose address is no longer in
    // the new schema. Build a set of new addresses first.
    std::unordered_map<std::string, ui::Control*> newByAddress;
    newByAddress.reserve (schema.flatControls.size());
    for (auto* c : schema.flatControls)
        newByAddress[c->address] = c;

    for (auto it = impl->addressToBinding.begin(); it != impl->addressToBinding.end(); )
    {
        if (newByAddress.find (it->first) == newByAddress.end())
        {
            impl->releaseSlot (it->second.cat, it->second.index);
            impl->slotIdToAddress.erase (slotIdFor (it->second.cat, it->second.index).toStdString());
            it = impl->addressToBinding.erase (it);
        }
        else
        {
            ++it;
        }
    }

    // Step 2: bind every control in the new schema. Reuse the existing
    // slot if the address was already bound; otherwise allocate.
    for (auto* c : schema.flatControls)
    {
        const auto cat = categoryFor (c->kind);
        auto it = impl->addressToBinding.find (c->address);
        if (it != impl->addressToBinding.end())
        {
            // Re-use, but update min/max in case the source's range
            // changed (Faust author edited the slider bounds).
            it->second.min = c->min;
            it->second.max = c->max;
            // Category mismatch (e.g. an address that used to be an
            // hslider is now a checkbox) — release the old slot and
            // re-allocate.
            if (it->second.cat != cat)
            {
                impl->releaseSlot (it->second.cat, it->second.index);
                impl->slotIdToAddress.erase (slotIdFor (it->second.cat, it->second.index).toStdString());
                const int idx = impl->allocateSlot (cat);
                if (idx < 0)
                {
                    impl->addressToBinding.erase (it);
                    c->slotId.clear();
                    continue;
                }
                it->second.cat   = cat;
                it->second.index = idx;
                impl->slotIdToAddress[slotIdFor (cat, idx).toStdString()] = c->address;
            }
            c->slotId = slotIdFor (it->second.cat, it->second.index).toStdString();
        }
        else
        {
            const int idx = impl->allocateSlot (cat);
            if (idx < 0)
            {
                // Over-budget — leave the control unbound. The webview
                // will see an empty slotId.
                c->slotId.clear();
                continue;
            }
            Impl::Binding b;
            b.cat   = cat;
            b.index = idx;
            b.min   = c->min;
            b.max   = c->max;
            impl->addressToBinding[c->address] = b;
            impl->slotIdToAddress[slotIdFor (cat, idx).toStdString()] = c->address;
            c->slotId = slotIdFor (cat, idx).toStdString();

            // NEW binding only — queue the schema's init value for an
            // APVTS write once the lock is released. Existing bindings
            // are deliberately left alone so the user's saved/in-flight
            // value survives hot recompiles and DAW state recall. The
            // slot-pool layout defaults every slot's APVTS value to 0,
            // which without this step renders as the min end of the
            // range — so a Faust source like `hslider("cutoff", 6000,
            // 80, 18000, 1)` would come up at 80 Hz instead of 6 kHz
            // on first compile. Bargraphs are read-only from APVTS's
            // side and have no init to seed.
            if (cat != SlotCategory::Bargraph)
            {
                const float normalised = (cat == SlotCategory::Bool)
                                            ? (c->init > 0.5f ? 1.0f : 0.0f)
                                            : normalise (c->init, c->min, c->max);
                pendingInits.push_back ({ slotIdFor (cat, idx), normalised });
            }
        }
    }

    // Step 3: seed the new DSP instance with the current APVTS slot
    // values for every bound float/bool address. Bargraphs are read-
    // only from Faust's perspective; nothing to seed there.
    for (const auto& kv : impl->addressToBinding)
    {
        const auto&        address = kv.first;
        const auto&        b       = kv.second;
        const auto         id      = slotIdFor (b.cat, b.index);
        if (b.cat == SlotCategory::Bargraph)
            continue;
        if (auto* raw = impl->apvts.getRawParameterValue (id))
        {
            const float normalised = raw->load();
            if (b.cat == SlotCategory::Float)
                impl->runtime.setZone (address, denormalise (normalised, b.min, b.max));
            else // Bool
                impl->runtime.setZone (address, normalised > 0.5f ? 1.0f : 0.0f);
        }
    }

    // Step 4: rebuild the bargraph snapshot. Cache the APVTS atomic
    // pointer, the raw Faust zone pointer (so the audio thread reads
    // without locking the runtime), and the min/max for normalisation.
    auto& dest = impl->useBufferA ? impl->bargraphBufferB : impl->bargraphBufferA;
    dest.clear();
    for (const auto& kv : impl->addressToBinding)
    {
        if (kv.second.cat != SlotCategory::Bargraph)
            continue;
        BargraphBinding bb;
        bb.min       = kv.second.min;
        bb.range     = std::max (1e-9f, kv.second.max - kv.second.min);
        bb.paramAtom = impl->apvts.getRawParameterValue (slotIdFor (kv.second.cat, kv.second.index));
        bb.zonePtr   = impl->runtime.getZonePointer (kv.first);
        if (bb.paramAtom != nullptr && bb.zonePtr != nullptr)
            dest.push_back (bb);
    }
    impl->useBufferA = ! impl->useBufferA;
    impl->bargraphView.store (&dest, std::memory_order_release);
    } // release impl->mutex

    // Apply queued init writes via `setValueNotifyingHost`. The listener
    // chain re-enters `parameterChanged` (which re-acquires the mutex),
    // and also propagates to the WebView relays so the JS side renders
    // the init position immediately. Step 3 above already pushed the
    // current APVTS value into the DSP, but at that point the new slot
    // still held its 0-default — this `setValueNotifyingHost` call is
    // what makes the listener-chained `parameterChanged` write the true
    // init value into the live zone.
    for (const auto& p : pendingInits)
    {
        if (auto* parameter = impl->apvts.getParameter (p.slotId))
            parameter->setValueNotifyingHost (p.normalised);
    }
}

void ParamBridge::readBargraphsIntoSlots() noexcept
{
    // AUDIO THREAD. No locks. The bargraph view pointer is published by
    // rebindForSchema under release ordering; the contents are stable
    // for the duration of the view's tenure as the front buffer. Zone
    // pointers cached in each binding are valid because:
    //
    // - The compile worker swaps `currentInstance` and the message
    //   thread's compile callback (which re-runs `rebindForSchema`)
    //   overwrites the cached pointers BEFORE `runtime.drainDeletions`
    //   frees the previous DSP. Until that rebind runs, the old binding
    //   vector is still the front buffer — its zone pointers still
    //   point into the previous (not-yet-freed) DSP, which is fine.
    //
    // - In the brief window between worker-swap and message-thread
    //   rebind, the audio thread reads stale-but-valid bargraph values
    //   from the previous DSP. One tick of stale meter data is correct
    //   (never UB).
    const auto* view = impl->bargraphView.load (std::memory_order_acquire);
    if (view == nullptr)
        return;
    for (const auto& bb : *view)
    {
        if (bb.zonePtr == nullptr || bb.paramAtom == nullptr)
            continue;
        const float raw = *bb.zonePtr;
        const float n   = std::clamp ((raw - bb.min) / bb.range, 0.0f, 1.0f);
        bb.paramAtom->store (n, std::memory_order_relaxed);
    }
}

juce::ValueTree ParamBridge::saveMapping() const
{
    std::lock_guard<std::mutex> lock (impl->mutex);
    juce::ValueTree tree ("ParamBridge");
    for (const auto& kv : impl->addressToBinding)
    {
        juce::ValueTree entry ("Binding");
        entry.setProperty ("address", juce::String (kv.first), nullptr);
        entry.setProperty ("slotId",
                           slotIdFor (kv.second.cat, kv.second.index),
                           nullptr);
        entry.setProperty ("min", kv.second.min, nullptr);
        entry.setProperty ("max", kv.second.max, nullptr);
        tree.appendChild (entry, nullptr);
    }
    return tree;
}

void ParamBridge::restoreMapping (const juce::ValueTree& tree)
{
    std::lock_guard<std::mutex> lock (impl->mutex);

    // Reset everything; the persisted state is the truth.
    impl->addressToBinding.clear();
    impl->slotIdToAddress.clear();
    std::fill (impl->floatTaken.begin(),    impl->floatTaken.end(),    false);
    std::fill (impl->boolTaken.begin(),     impl->boolTaken.end(),     false);
    std::fill (impl->bargraphTaken.begin(), impl->bargraphTaken.end(), false);
    impl->reserveBypassSlot();

    if (! tree.hasType ("ParamBridge"))
        return;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto entry = tree.getChild (i);
        const auto address = entry.getProperty ("address").toString().toStdString();
        const auto slotId  = entry.getProperty ("slotId").toString();
        SlotCategory cat;
        int idx;
        if (! parseSlotId (slotId, cat, idx))
            continue;
        if (impl->isReserved (cat, idx))
            continue;
        Impl::Binding b;
        b.cat   = cat;
        b.index = idx;
        b.min   = (float) (double) entry.getProperty ("min", 0.0);
        b.max   = (float) (double) entry.getProperty ("max", 1.0);
        impl->addressToBinding[address] = b;
        impl->slotIdToAddress[slotId.toStdString()] = address;
        impl->markTaken (cat, idx);
    }
}

void ParamBridge::snapshotBargraphsInto (std::array<float, kBargraphSlotCount>& out) const noexcept
{
    // AUDIO THREAD. Read every bargraph slot's APVTS raw atomic into the
    // output array. The atomics are owned by APVTS and live as long as
    // the processor; pointers were cached at construction. Unbound slots
    // read whatever value last lived in the atomic (0 by default since
    // the slots default to 0 and `readBargraphsIntoSlots` only writes
    // bound slots).
    for (int i = 0; i < kBargraphSlotCount; ++i)
    {
        const auto* atom = impl->bargraphSlotAtoms[(size_t) i];
        out[(size_t) i] = (atom != nullptr) ? atom->load (std::memory_order_relaxed) : 0.0f;
    }
}

void ParamBridge::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Bypass: ignored here — handled directly in PluginProcessor.
    if (parameterID == boolSlotId (kBypassSlotIndex))
        return;

    std::string  address;
    SlotCategory cat = SlotCategory::Float;
    float        min = 0.0f, max = 1.0f;
    {
        std::lock_guard<std::mutex> lock (impl->mutex);
        const auto addressIt = impl->slotIdToAddress.find (parameterID.toStdString());
        if (addressIt == impl->slotIdToAddress.end())
            return;
        const auto bindIt = impl->addressToBinding.find (addressIt->second);
        if (bindIt == impl->addressToBinding.end())
            return;
        address = addressIt->second;
        cat     = bindIt->second.cat;
        min     = bindIt->second.min;
        max     = bindIt->second.max;
    }

    const float denorm = (cat == SlotCategory::Bool)
                            ? (newValue > 0.5f ? 1.0f : 0.0f)
                            : denormalise (newValue, min, max);

    impl->runtime.setZone (address, denorm);
}

} // namespace crucible
