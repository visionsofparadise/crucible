#include "SourceLoader.h"

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

namespace crucible {

namespace
{
    // Lightweight content-change fingerprint. Combines mtime + size +
    // a 64-bit hash of the loaded text. Any mismatch versus the last
    // seen fingerprint triggers OnSourceChanged. Hash collisions on
    // unchanged contents are statistically irrelevant for this use
    // (the mtime + size fields catch the common case; the hash
    // disambiguates same-second writes that preserve size).
    struct Fingerprint
    {
        juce::int64 mtimeMs = 0;
        juce::int64 size    = 0;
        juce::int64 hash    = 0;

        bool operator!= (const Fingerprint& other) const noexcept
        {
            return mtimeMs != other.mtimeMs
                || size    != other.size
                || hash    != other.hash;
        }
    };
}

struct SourceLoader::Impl
{
    OnSourceChanged onChanged;
    OnError         onError;

    juce::File   currentPath;
    juce::String editorBuffer;

    // Tracking state for the watcher. `seenExists` tracks whether the
    // last poll observed the file as present, to debounce missing-file
    // events. `lastFingerprint` is the most recent (mtime, size, hash)
    // tuple seen with the file present; only used while seenExists.
    bool        seenExists = false;
    Fingerprint lastFingerprint;

    // The file dialog must outlive the user's interaction with it; the
    // launchAsync callback fires when the dialog closes. Hold the
    // current chooser as a member so successive open / save requests
    // safely cancel any in-flight dialog by replacing the unique_ptr.
    std::unique_ptr<juce::FileChooser> activeChooser;

    void emitChanged (const juce::String& source, const juce::File& path)
    {
        if (onChanged)
            onChanged (source, path);
    }

    void emitError (SourceLoader::InputError::Kind kind, const juce::String& message)
    {
        if (onError)
            onError ({ kind, message });
    }
};

SourceLoader::SourceLoader()
    : impl (std::make_unique<Impl>())
{
    // 200 ms polling — fast enough for "feels instant" hot reload,
    // slow enough that the timer thread cost is negligible. The timer
    // is started unconditionally; the callback no-ops while no path
    // is set.
    startTimer (200);
}

SourceLoader::~SourceLoader()
{
    stopTimer();
}

void SourceLoader::setCallbacks (OnSourceChanged onChanged, OnError onError)
{
    impl->onChanged = std::move (onChanged);
    impl->onError   = std::move (onError);
}

void SourceLoader::requestOpen (juce::Component* parentForDialog)
{
    juce::ignoreUnused (parentForDialog);

    auto chooser = std::make_unique<juce::FileChooser> (
        "Open Faust source",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.dsp");

    auto* raw = chooser.get();
    impl->activeChooser = std::move (chooser);

    raw->launchAsync (
        juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File() || ! file.existsAsFile())
            {
                // User cancelled, or selection was a non-file. No-op.
                impl->activeChooser.reset();
                return;
            }

            const auto contents = file.loadFileAsString();
            // `loadFileAsString` returns an empty string for both
            // "empty file" and "read failed" — distinguish by stat.
            if (contents.isEmpty() && ! file.existsAsFile())
            {
                impl->emitError (InputError::Kind::FileReadFailed,
                                 "Failed to read: " + file.getFullPathName());
                impl->activeChooser.reset();
                return;
            }

            impl->currentPath  = file;
            impl->editorBuffer = contents;

            // Prime the watcher baseline so the loaded contents do not
            // trigger a redundant OnSourceChanged on the next poll.
            impl->seenExists           = true;
            impl->lastFingerprint.mtimeMs = file.getLastModificationTime().toMilliseconds();
            impl->lastFingerprint.size    = file.getSize();
            impl->lastFingerprint.hash    = static_cast<juce::int64> (contents.hashCode64());

            impl->emitChanged (contents, file);
            impl->activeChooser.reset();
        });
}

void SourceLoader::requestSave (const juce::String& bufferContents,
                                juce::Component*    parentForDialog)
{
    juce::ignoreUnused (parentForDialog);

    impl->editorBuffer = bufferContents;

    const auto defaultLocation = (impl->currentPath != juce::File())
        ? impl->currentPath
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
              .getChildFile ("untitled.dsp");

    auto chooser = std::make_unique<juce::FileChooser> (
        "Save Faust source as",
        defaultLocation,
        "*.dsp");

    auto* raw = chooser.get();
    impl->activeChooser = std::move (chooser);

    raw->launchAsync (
        juce::FileBrowserComponent::saveMode
      | juce::FileBrowserComponent::canSelectFiles
      | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, bufferContents] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
            {
                impl->activeChooser.reset();
                return;
            }

            if (! file.replaceWithText (bufferContents))
            {
                impl->emitError (InputError::Kind::FileWriteFailed,
                                 "Failed to write: " + file.getFullPathName());
                impl->activeChooser.reset();
                return;
            }

            // Update the watched path. The watcher's next poll picks up
            // the new file and emits OnSourceChanged through the
            // standard mtime/size/hash-mismatch path, ensuring SAVE
            // produces exactly one recompile (the watcher-driven one).
            impl->currentPath = file;
            // Reset the baseline so the next poll observes the change.
            impl->seenExists      = false;
            impl->lastFingerprint = Fingerprint{};
            impl->activeChooser.reset();
        });
}

void SourceLoader::setEditorBuffer (const juce::String& contents)
{
    impl->editorBuffer = contents;
}

void SourceLoader::restorePath (const juce::File& path)
{
    impl->currentPath = path;
    impl->seenExists      = false;
    impl->lastFingerprint = Fingerprint{};

    if (path == juce::File())
        return;

    if (path.existsAsFile())
    {
        const auto contents = path.loadFileAsString();
        impl->editorBuffer = contents;

        impl->seenExists           = true;
        impl->lastFingerprint.mtimeMs = path.getLastModificationTime().toMilliseconds();
        impl->lastFingerprint.size    = path.getSize();
        impl->lastFingerprint.hash    = static_cast<juce::int64> (contents.hashCode64());

        impl->emitChanged (contents, path);
    }
    else
    {
        impl->emitError (InputError::Kind::FileMissing,
                         "Watched file not found: " + path.getFullPathName());
    }
}

juce::File SourceLoader::currentPath() const
{
    return impl->currentPath;
}

juce::String SourceLoader::currentBuffer() const
{
    return impl->editorBuffer;
}

void SourceLoader::timerCallback()
{
    if (impl->currentPath == juce::File())
        return;

    const auto& path = impl->currentPath;

    if (! path.existsAsFile())
    {
        if (impl->seenExists)
        {
            // Transition exists → missing. Emit once.
            impl->seenExists = false;
            impl->emitError (InputError::Kind::FileMissing,
                             "Watched file not found: " + path.getFullPathName());
        }
        // While missing, do not spam.
        return;
    }

    Fingerprint current;
    current.mtimeMs = path.getLastModificationTime().toMilliseconds();
    current.size    = path.getSize();

    // Skip the hash on the fast path when mtime + size already match —
    // avoids re-reading the file every 200 ms when nothing has changed.
    if (impl->seenExists
        && current.mtimeMs == impl->lastFingerprint.mtimeMs
        && current.size    == impl->lastFingerprint.size)
    {
        return;
    }

    const auto contents = path.loadFileAsString();
    current.hash = static_cast<juce::int64> (contents.hashCode64());

    const bool wasMissing = ! impl->seenExists;
    const bool changed    = wasMissing || (current != impl->lastFingerprint);

    impl->seenExists      = true;
    impl->lastFingerprint = current;

    if (changed)
    {
        impl->editorBuffer = contents;
        impl->emitChanged (contents, path);
    }
}

} // namespace crucible
