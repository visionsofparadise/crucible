// Phase 1 ABI smoke test: verify libfaustwithllvm links against Crucible's
// chosen toolchain on the current platform, JIT-compiles a passthrough
// Faust program, and produces correct output. If this fails to *link* the
// LLVM symbols, ABI compatibility with Faust's prebuilt static archive is
// broken on this platform and the fallback (build libfaust+LLVM ourselves
// or switch the platform's compiler) needs to be invoked. See
// plan-bootstrap-vst.md Phase 1 escalation note.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <faust/dsp/llvm-dsp.h>

#include <string>
#include <vector>

TEST_CASE ("libfaust LLVM JIT compiles passthrough", "[smoke]")
{
    std::string err;
    const std::string source = "process = _;";
    std::vector<const char*> argv {};
    llvm_dsp_factory* factory = createDSPFactoryFromString (
        "smoke", source,
        static_cast<int> (argv.size()), argv.data(),
        "" /* target */, err, -1 /* opt */);
    REQUIRE (factory != nullptr);
    REQUIRE (err.empty());

    llvm_dsp* dsp = factory->createDSPInstance();
    REQUIRE (dsp != nullptr);

    const int sampleRate = 48000;
    const int frames = 64;
    dsp->init (sampleRate);
    REQUIRE (dsp->getNumInputs() == 1);
    REQUIRE (dsp->getNumOutputs() == 1);

    std::vector<float> in (static_cast<size_t> (frames), 0.5f);
    std::vector<float> out (static_cast<size_t> (frames), 0.0f);
    float* inP = in.data();
    float* outP = out.data();
    dsp->compute (frames, &inP, &outP);

    for (int i = 0; i < frames; ++i)
        REQUIRE (out[static_cast<size_t> (i)] == Catch::Approx (0.5f).margin (1e-6f));

    delete dsp;
    deleteDSPFactory (factory);
}
