// Phase 4.1 verification: SourceLoader watches the configured path,
// emits OnSourceChanged on external content changes, and emits
// OnError(FileMissing) when the watched file disappears.
//
// Test strategy: SourceLoader uses juce::Timer, which requires an
// active message loop on the thread that owns the timers. The headless
// build sets JUCE_MODAL_LOOPS_PERMITTED=0, so runDispatchLoopUntil is
// unavailable; once stopDispatchLoop() is called, subsequent
// runDispatchLoop() invocations return immediately and the loop cannot
// be restarted in the same process.
//
// The test therefore drives the entire scenario through a single
// runDispatchLoop() invocation, using a state-machine driver Timer
// that progresses through the test stages on the message thread and
// signals success/failure to the test thread via atomics. The test
// thread blocks until stopDispatchLoop() is posted from the driver.

#include <catch2/catch_test_macros.hpp>

#include "SourceLoader.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    enum class Stage
    {
        WaitingForUpdate,
        WaitingForMissing,
        WaitingForNoSpam,
        WaitingForReturn,
        Done
    };

    struct Driver : public juce::Timer
    {
        // Inputs
        juce::File   tempFile;
        juce::String updatedContents;
        crucible::SourceLoader* loader = nullptr;
        std::atomic<int>* changeCount = nullptr;
        std::atomic<int>* errorCount  = nullptr;
        juce::String* lastSource = nullptr;

        // Outputs (read by the test thread after stopDispatchLoop)
        std::atomic<bool> updateSeen   { false };
        std::atomic<bool> missingSeen  { false };
        std::atomic<bool> noSpamPassed { false };
        std::atomic<bool> returnSeen   { false };
        std::atomic<int>  errorsAtMissing { 0 };
        std::atomic<int>  changesAtMissing { 0 };

        // The contents to write when bringing the file back.
        juce::String returnContents;

        Stage stage = Stage::WaitingForUpdate;
        std::chrono::steady_clock::time_point stageDeadline;
        std::chrono::steady_clock::time_point noSpamUntil;

        void start()
        {
            // Stage 1: write the updated contents and wait up to 2 s
            // for the watcher to observe the rewrite. mtime resolution
            // on Windows can collapse same-second writes; the content
            // hash also catches that case. Pass "\n" line-endings so
            // the on-disk bytes match the string literal — JUCE's
            // default of "\r\n" would make round-tripped contents
            // compare unequal to the source literal.
            const auto written = tempFile.replaceWithText (updatedContents, false, false, "\n");
            jassert (written);
            juce::ignoreUnused (written);

            stageDeadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (2000);
            startTimer (25);
        }

        void timerCallback() override
        {
            const auto now = std::chrono::steady_clock::now();

            switch (stage)
            {
                case Stage::WaitingForUpdate:
                {
                    if (changeCount->load() >= 2 && *lastSource == updatedContents)
                    {
                        updateSeen.store (true);
                        // Stage 2: delete the file; wait for FileMissing error.
                        const auto deleted = tempFile.deleteFile();
                        jassert (deleted);
                        juce::ignoreUnused (deleted);

                        stage = Stage::WaitingForMissing;
                        stageDeadline = now + std::chrono::milliseconds (2000);
                    }
                    else if (now >= stageDeadline)
                    {
                        finish();
                    }
                    return;
                }

                case Stage::WaitingForMissing:
                {
                    if (errorCount->load() >= 1)
                    {
                        missingSeen.store (true);
                        errorsAtMissing.store (errorCount->load());
                        changesAtMissing.store (changeCount->load());
                        // Stage 3: verify no further errors fire over 700 ms.
                        stage = Stage::WaitingForNoSpam;
                        noSpamUntil = now + std::chrono::milliseconds (700);
                    }
                    else if (now >= stageDeadline)
                    {
                        finish();
                    }
                    return;
                }

                case Stage::WaitingForNoSpam:
                {
                    if (now >= noSpamUntil)
                    {
                        noSpamPassed.store (errorCount->load() == errorsAtMissing.load());

                        // Stage 4: write the file back and wait up to
                        // 2 s for the watcher to fire OnSourceChanged
                        // again (the missing → exists transition has
                        // no separate "recovered" callback — the
                        // source-changed signal is the recovery).
                        const auto written = tempFile.replaceWithText (
                            returnContents, false, false, "\n");
                        jassert (written);
                        juce::ignoreUnused (written);

                        stage = Stage::WaitingForReturn;
                        stageDeadline = now + std::chrono::milliseconds (2000);
                    }
                    return;
                }

                case Stage::WaitingForReturn:
                {
                    if (changeCount->load() > changesAtMissing.load()
                        && *lastSource == returnContents)
                    {
                        returnSeen.store (true);
                        finish();
                    }
                    else if (now >= stageDeadline)
                    {
                        finish();
                    }
                    return;
                }

                case Stage::Done:
                    return;
            }
        }

        void finish()
        {
            stage = Stage::Done;
            stopTimer();
            // The test thread pumps the loop in bounded slices via
            // runDispatchLoopUntil and exits when `stage` reaches Done;
            // no stopDispatchLoop() (whose one-shot semantics vary by
            // platform and would poison later pumps).
        }
    };
}

TEST_CASE ("SourceLoader emits on external content changes and missing files",
           "[source-loader]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("crucible_source_loader_test_"
                                    + juce::Uuid().toDashedString()
                                    + ".dsp");

    const juce::String initialContents = "process = _;\n";
    REQUIRE (tempFile.replaceWithText (initialContents, false, false, "\n"));

    crucible::SourceLoader loader;

    std::atomic<int>           changeCount { 0 };
    std::atomic<int>           errorCount  { 0 };
    juce::String               lastSource;
    crucible::SourceLoader::InputError::Kind lastErrorKind {};

    loader.setCallbacks (
        [&] (const juce::String& source, const juce::File&)
        {
            lastSource = source;
            ++changeCount;
        },
        [&] (const crucible::SourceLoader::InputError& err)
        {
            lastErrorKind = err.kind;
            ++errorCount;
        });

    // restorePath fires OnSourceChanged synchronously with the loaded
    // contents and primes the watcher baseline. This runs on the test
    // thread before the dispatch loop starts, which is fine because
    // the callbacks are invoked synchronously here (not via the timer).
    loader.restorePath (tempFile);
    REQUIRE (changeCount.load() == 1);
    REQUIRE (lastSource == initialContents);

    // Brief delay so the file's mtime moves on the rewrite below;
    // Windows' second-resolution mtime can otherwise collide with the
    // initial write and the watcher would fall back to the content
    // hash. The hash works, but moving the mtime exercises the fast
    // path.
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    Driver driver;
    driver.tempFile        = tempFile;
    driver.updatedContents = "process = _ * 0.5;\n";
    driver.returnContents  = "process = _ * 0.25;\n";
    driver.loader          = &loader;
    driver.changeCount     = &changeCount;
    driver.errorCount      = &errorCount;
    driver.lastSource      = &lastSource;
    driver.start();

    // Pump the message loop in bounded slices until the Driver's state
    // machine reaches Done (it calls finish() on success or per-stage
    // timeout) or a hard deadline elapses. runDispatchLoopUntil (enabled
    // via JUCE_MODAL_LOOPS_PERMITTED on the test target) re-enters the
    // loop each slice, so it drives the timers reliably on every
    // platform — unlike a single runDispatchLoop(), which returns early
    // on macOS's headless run loop and leaves the scenario unfinished.
    auto* mm = juce::MessageManager::getInstance();
    const auto hardDeadlineMs = juce::Time::getMillisecondCounter() + 15000;
    while (driver.stage != Stage::Done
           && juce::Time::getMillisecondCounter() < hardDeadlineMs)
    {
        mm->runDispatchLoopUntil (50);
    }

    REQUIRE (driver.updateSeen.load());
    REQUIRE (driver.missingSeen.load());
    REQUIRE (lastErrorKind == crucible::SourceLoader::InputError::Kind::FileMissing);
    REQUIRE (driver.noSpamPassed.load());
    // Recovery: file came back, OnSourceChanged fired with the new
    // contents, and no separate "recovered" event was needed.
    REQUIRE (driver.returnSeen.load());
    REQUIRE (lastSource == driver.returnContents);
}
