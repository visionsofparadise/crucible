#pragma once

#include <functional>
#include <memory>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace juce { class Component; }

namespace crucible {

// SourceLoader — owns the watched `.dsp` path, the editor working buffer,
// and a 200 ms polling timer that emits `OnSourceChanged` whenever the
// watched file's contents change on disk (mtime / size / CRC32 of contents).
//
// Threading model:
//   - All public methods are MESSAGE THREAD only.
//   - The polling timer runs on the message thread (juce::Timer).
//   - Construction installs no path; call `restorePath` (project reload)
//     or `requestOpen` (user OPEN dialog) to start watching.
//   - Callbacks fire on the message thread.
//
// Save semantics:
//   - `requestSave` always opens a Save-As dialog (per
//     `design-vst.md` "Save = always Save As" decision). On a successful
//     write, the watched path is updated to the chosen path; the watcher
//     picks up the file change on its next poll and emits OnSourceChanged
//     through the standard path. `requestSave` itself does NOT emit
//     OnSourceChanged.
//
// Missing-file semantics:
//   - On the transition exists → missing, OnError(FileMissing) fires once.
//   - On the transition missing → exists, OnSourceChanged fires with the
//     new contents (no separate "recovered" event kind; the source-changed
//     callback is the recovery signal).
//   - While the file remains missing, no further events fire (no spam).
class SourceLoader : private juce::Timer
{
public:
    struct InputError
    {
        enum class Kind { FileMissing, FileReadFailed, FileWriteFailed };
        Kind         kind;
        juce::String message;
    };

    using OnSourceChanged = std::function<void (const juce::String& source,
                                                const juce::File&   path)>;
    using OnError         = std::function<void (const InputError&)>;

    SourceLoader();
    ~SourceLoader() override;

    // Install message-thread callbacks. Either may be null; an
    // unconfigured callback is silently dropped.
    void setCallbacks (OnSourceChanged onChanged, OnError onError);

    // OPEN: launch a `.dsp` file dialog. On selection, read the file,
    // set the watched path + editor buffer, emit OnSourceChanged. The
    // dialog is async; this method returns immediately.
    void requestOpen (juce::Component* parentForDialog);

    // SAVE-As: launch a Save dialog defaulting to the current watched
    // path (or the user data directory if unset). On selection, write
    // `bufferContents` to the chosen path and update the watched path.
    // The watcher emits OnSourceChanged on the next poll; this method
    // does not emit directly. Async; returns immediately.
    void requestSave (const juce::String& bufferContents,
                      juce::Component*    parentForDialog);

    // Set the editor working buffer (typed in-VST, not yet saved). Used
    // as the default contents in the SaveAs dialog and exposed via
    // `currentBuffer()` so the webview's UI state survives a plugin
    // editor close/reopen.
    void setEditorBuffer (const juce::String& contents);

    // Restore the watched path on plugin state load. If the file exists,
    // reads it, updates the editor buffer, primes the watcher's
    // baseline, and emits OnSourceChanged. If the file is missing,
    // emits OnError(FileMissing). Either way, polling starts.
    //
    // Emits SYNCHRONOUSLY on the calling thread, before returning. The
    // callback target must be fully constructed before this call.
    void restorePath (const juce::File& path);

    juce::File   currentPath()   const;
    juce::String currentBuffer() const;

private:
    void timerCallback() override;

    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace crucible
