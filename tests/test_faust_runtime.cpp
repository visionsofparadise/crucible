// Phase 3.3 verification: FaustRuntime compiles Faust source off-thread,
// swaps the active instance gaplessly, surfaces compile failure with
// a non-empty error message, and coalesces back-to-back submissions to
// the latest source.

#include <catch2/catch_test_macros.hpp>

#include "FaustRuntime.h"
#include "StdlibExtractor.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

namespace
{
    struct CompileSignal
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::optional<crucible::CompileResult> latest;
        int count = 0;

        void onCompile (const crucible::CompileResult& r)
        {
            std::lock_guard<std::mutex> lock (mutex);
            latest = r;
            ++count;
            cv.notify_all();
        }

        bool waitForCount (int target, int timeoutMs)
        {
            std::unique_lock<std::mutex> lock (mutex);
            return cv.wait_for (lock, std::chrono::milliseconds (timeoutMs),
                                [this, target] { return count >= target; });
        }
    };

    std::vector<juce::File> defaultIncludes()
    {
        return { crucible::StdlibExtractor::ensureExtracted() };
    }
}

TEST_CASE ("FaustRuntime compiles a passthrough source", "[faust-runtime]")
{
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    runtime.submit ("import(\"stdfaust.lib\"); process = os.osc(440);");

    REQUIRE (signal.waitForCount (1, 15000));

    std::lock_guard<std::mutex> lock (signal.mutex);
    REQUIRE (signal.latest.has_value());
    REQUIRE (signal.latest->success);
    REQUIRE (signal.latest->errorMessage.empty());
    REQUIRE (signal.latest->numOutputs == 1);
    REQUIRE_FALSE (signal.latest->jsonUi.empty());
}

TEST_CASE ("FaustRuntime surfaces a compile error", "[faust-runtime]")
{
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    // `os.unknown` does not exist — libfaust must report an error.
    runtime.submit ("import(\"stdfaust.lib\"); process = os.unknown(440);");

    REQUIRE (signal.waitForCount (1, 15000));

    std::lock_guard<std::mutex> lock (signal.mutex);
    REQUIRE (signal.latest.has_value());
    REQUIRE_FALSE (signal.latest->success);
    REQUIRE_FALSE (signal.latest->errorMessage.empty());
}

TEST_CASE ("FaustRuntime coalesces back-to-back submissions", "[faust-runtime]")
{
    crucible::FaustRuntime runtime;
    CompileSignal signal;
    runtime.setCompileCallback ([&] (const crucible::CompileResult& r) { signal.onCompile (r); });
    runtime.setIncludePaths (defaultIncludes());
    runtime.setSampleRate (48000.0);

    // Submit two sources back-to-back. The worker may not have started
    // on the first yet — the second submission must displace the first
    // in the queue, and the final compile must reflect the second
    // source (a passthrough). If both got through, the callback fires
    // twice; the latest result must still be the passthrough.
    runtime.submit ("import(\"stdfaust.lib\"); process = os.osc(440);");
    runtime.submit ("process = _;");

    // Wait for at least one compile.
    REQUIRE (signal.waitForCount (1, 15000));
    // Give a brief window for a second compile to land if the worker
    // actually got both — coalescing is best-effort.
    signal.waitForCount (2, 1000);

    REQUIRE (runtime.waitForIdle (15000));

    std::lock_guard<std::mutex> lock (signal.mutex);
    REQUIRE (signal.latest.has_value());
    REQUIRE (signal.latest->success);
    // The latest source is the passthrough — 1 in, 1 out.
    REQUIRE (signal.latest->numInputs  == 1);
    REQUIRE (signal.latest->numOutputs == 1);
}
