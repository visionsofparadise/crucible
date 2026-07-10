#include "StdlibExtractor.h"
#include "CrucibleStdlibData.h"

#include <cstring>

#ifndef CRUCIBLE_FAUST_VERSION
#define CRUCIBLE_FAUST_VERSION "unknown"
#endif

namespace crucible {

namespace
{
    juce::File extractionRoot()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        return base.getChildFile ("ZCROSS")
                   .getChildFile ("Crucible")
                   .getChildFile (juce::String ("stdlib-") + CRUCIBLE_FAUST_VERSION);
    }

    // Original bundle filenames look like "stdfaust.faustlib" (we
    // renamed `.lib` → `.faustlib` at bundle time in CMake). Restore
    // the `.lib` extension expected by libfaust.
    juce::String materialisedName (const juce::String& originalFilename)
    {
        const juce::String suffix (".faustlib");
        if (originalFilename.endsWith (suffix))
            return originalFilename.dropLastCharacters (suffix.length()) + ".lib";
        return originalFilename;
    }
}

juce::File StdlibExtractor::ensureExtracted()
{
    const auto dir = extractionRoot();
    dir.createDirectory();

    const int numFiles = CrucibleStdlibData::namedResourceListSize;

    // Idempotency check: same file count + each destination exists with
    // matching size. Skip extraction if everything lines up.
    bool needsExtract = false;
    for (int i = 0; i < numFiles; ++i)
    {
        const char* bundleName     = CrucibleStdlibData::namedResourceList[i];
        const char* originalName   = CrucibleStdlibData::originalFilenames[i];
        const auto  target         = dir.getChildFile (materialisedName (originalName));

        int dataSize = 0;
        const char* data = CrucibleStdlibData::getNamedResource (bundleName, dataSize);
        if (data == nullptr)
            continue;

        if (! target.existsAsFile() || target.getSize() != static_cast<juce::int64> (dataSize))
        {
            needsExtract = true;
            break;
        }
    }

    if (! needsExtract)
        return dir;

    for (int i = 0; i < numFiles; ++i)
    {
        const char* bundleName   = CrucibleStdlibData::namedResourceList[i];
        const char* originalName = CrucibleStdlibData::originalFilenames[i];
        const auto  target       = dir.getChildFile (materialisedName (originalName));

        int dataSize = 0;
        const char* data = CrucibleStdlibData::getNamedResource (bundleName, dataSize);
        if (data == nullptr || dataSize <= 0)
            continue;

        juce::FileOutputStream out (target);
        if (! out.openedOk())
            continue;

        out.setPosition (0);
        out.truncate();
        out.write (data, static_cast<size_t> (dataSize));
        out.flush();
    }

    return dir;
}

} // namespace crucible
