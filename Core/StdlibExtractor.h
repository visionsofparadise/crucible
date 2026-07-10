#pragma once

#include <juce_core/juce_core.h>

namespace crucible {

// Materialises Faust's standard library (bundled into the plugin via
// `juce_add_binary_data`) onto disk under
// `{userApplicationDataDirectory}/ZCROSS/Crucible/stdlib-{FAUST_VERSION}/`,
// then returns the directory. `.faustlib` extensions (used inside the
// bundle to dodge CMake's static-library link-input heuristic) are
// restored to `.lib` so libfaust resolves `import("stdfaust.lib");`
// transparently.
//
// The function is idempotent: on subsequent calls (within the same
// process or across reboots), if every bundled file already exists in
// the destination directory with the same size, no work is done.
//
// Returns the directory to be passed to libfaust as `-I` at compile
// time. Safe to call from any thread; concurrent calls race only on
// the file-create step, which is a benign overwrite of identical
// contents.
class StdlibExtractor
{
public:
    static juce::File ensureExtracted();
};

} // namespace crucible
