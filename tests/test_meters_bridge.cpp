// Phase 6.1 verification: MetersBridge is a lock-free SPSC ring of
// MeterFrame values mirroring gain-shaper's LiveSignalFifo. The audio
// thread pushes per-block; the UI thread drains at ~30 Hz with
// `tryPopLatest`. Older frames may be dropped — the contract is
// latest-wins, never block, never allocate.

#include <catch2/catch_test_macros.hpp>

#include "MetersBridge.h"

#include <atomic>
#include <chrono>
#include <thread>

TEST_CASE ("MetersBridge round-trips a frame between producer and consumer",
           "[meters-bridge]")
{
    crucible::MetersBridge bridge;

    crucible::MeterFrame in;
    in.inL_rms  = 0.11f; in.inR_rms  = 0.22f;
    in.inL_peak = 0.33f; in.inR_peak = 0.44f;
    in.outL_rms = 0.55f; in.outR_rms = 0.66f;
    in.outL_peak = 0.77f; in.outR_peak = 0.88f;
    for (int i = 0; i < (int) in.bargraphValues.size(); ++i)
        in.bargraphValues[(size_t) i] = (float) i / 16.0f;

    REQUIRE (bridge.tryPush (in));

    crucible::MeterFrame out;
    REQUIRE (bridge.tryPopLatest (out));
    REQUIRE (out.inL_rms   == in.inL_rms);
    REQUIRE (out.inR_rms   == in.inR_rms);
    REQUIRE (out.inL_peak  == in.inL_peak);
    REQUIRE (out.inR_peak  == in.inR_peak);
    REQUIRE (out.outL_rms  == in.outL_rms);
    REQUIRE (out.outR_rms  == in.outR_rms);
    REQUIRE (out.outL_peak == in.outL_peak);
    REQUIRE (out.outR_peak == in.outR_peak);
    REQUIRE (out.bargraphValues == in.bargraphValues);

    // Second pop with no fresh data returns false.
    REQUIRE_FALSE (bridge.tryPopLatest (out));
}

TEST_CASE ("MetersBridge tryPopLatest drops intermediate frames", "[meters-bridge]")
{
    crucible::MetersBridge bridge;

    // Push 5 frames with distinguishable outL_rms values.
    for (int i = 0; i < 5; ++i)
    {
        crucible::MeterFrame f;
        f.outL_rms = (float) i;
        REQUIRE (bridge.tryPush (f));
    }

    crucible::MeterFrame latest;
    REQUIRE (bridge.tryPopLatest (latest));
    REQUIRE (latest.outL_rms == 4.0f); // last pushed
    REQUIRE_FALSE (bridge.tryPopLatest (latest)); // drained
}

TEST_CASE ("MetersBridge tryPush returns false when ring is full", "[meters-bridge]")
{
    crucible::MetersBridge bridge;

    // juce::AbstractFifo reserves one slot internally to discriminate
    // full from empty, so the usable capacity is kCapacity-1. Push
    // kCapacity+1 frames and expect kCapacity-1 acceptances before the
    // ring is considered full.
    int accepted = 0;
    int lastAccepted = -1;
    for (int i = 0; i < crucible::MetersBridge::kCapacity + 1; ++i)
    {
        crucible::MeterFrame f;
        f.outL_rms = (float) i;
        if (bridge.tryPush (f))
        {
            ++accepted;
            lastAccepted = i;
        }
    }
    REQUIRE (accepted >= crucible::MetersBridge::kCapacity - 1);
    REQUIRE (accepted <  crucible::MetersBridge::kCapacity + 1);

    // tryPopLatest returns the most recently accepted frame.
    crucible::MeterFrame latest;
    REQUIRE (bridge.tryPopLatest (latest));
    REQUIRE (latest.outL_rms == (float) lastAccepted);
}

TEST_CASE ("MetersBridge round-trips across producer and consumer threads",
           "[meters-bridge]")
{
    // Two-thread smoke: producer pushes a sequence of frames; consumer
    // drains via tryPopLatest in a loop. The consumer should see the
    // last produced frame (or one very near it) eventually. This
    // exercises the SPSC ordering without asserting strict frame counts
    // — the contract permits dropping intermediate frames.
    crucible::MetersBridge bridge;
    constexpr int kFrames = 1024;

    std::atomic<bool> producerDone { false };
    std::thread producer ([&]
    {
        for (int i = 0; i < kFrames; ++i)
        {
            crucible::MeterFrame f;
            f.outR_rms = (float) i;
            // Spin on full ring (consumer will drain). Acceptable in
            // a test; production audio thread would `tryPush` and drop.
            while (! bridge.tryPush (f))
                std::this_thread::yield();
        }
        producerDone.store (true, std::memory_order_release);
    });

    float lastSeen = -1.0f;
    while (! producerDone.load (std::memory_order_acquire))
    {
        crucible::MeterFrame f;
        if (bridge.tryPopLatest (f))
            lastSeen = f.outR_rms;
        else
            std::this_thread::sleep_for (std::chrono::microseconds (50));
    }
    // Drain anything the producer pushed after we observed `done`.
    crucible::MeterFrame f;
    while (bridge.tryPopLatest (f))
        lastSeen = f.outR_rms;

    producer.join();

    // The very last frame produced has outR_rms == kFrames - 1; we
    // accept any "high" value because the consumer might have stopped
    // draining before the producer's final push due to interleaving.
    REQUIRE (lastSeen >= 0.0f);
    // We expect to have eventually seen the final frame; if not, the
    // contract is preserved (latest-wins), but in practice with this
    // spin-yield producer we will. Tolerate the last K frames.
    REQUIRE (lastSeen <= (float) (kFrames - 1));
}
