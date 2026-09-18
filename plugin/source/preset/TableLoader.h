#pragma once

#include "../dsp/WavetableLibrary.h"

#include <juce_events/juce_events.h>

#include <array>
#include <atomic>
#include <functional>
#include <set>

namespace gnarl::preset
{

/**
    Generates wavetables off the message thread.

    THE PROBLEM THIS EXISTS FOR, measured rather than assumed: generating one
    wavetable costs about 21 ms at the old 2048-sample geometry and roughly
    2.5x that at the shipped one. A preset that changes both oscillators'
    tables therefore costs something over a tenth of a second on the MESSAGE
    thread, which is the thread drawing the UI. Auditioning a bank means
    clicking through preset after preset, so that cost lands on every click
    and the browser becomes something nobody scrolls.

    WHAT IT DOES NOT DO IS MAKE THE AUDIO THREAD WAIT. The audio thread only
    ever reads an already-published pointer, so while a table is being built
    it keeps playing the one it already had. The patch arrives in two steps -
    parameters immediately, the new table a moment later - instead of arriving
    late. That is audible, and it is the right trade: a preset whose filter and
    envelope are correct and whose oscillator is briefly the previous table is
    much closer to what the user asked for than a frozen window.

    THE THREAD IS A juce::Thread AND NOT A ThreadPool JOB because it has to be
    stoppable: a user clicking down a bank queues a request per click, and the
    only one that matters is the last. Each request supersedes the one before
    it, and the generation loop checks threadShouldExit between tables.

    Nothing here is real-time safe and nothing here is called from the audio
    thread.
*/
class TableLoader : private juce::Thread,
                    private juce::AsyncUpdater
{
public:
    /** Called on the MESSAGE THREAD once the requested tables exist. */
    using OnReady = std::function<void()>;

    explicit TableLoader (dsp::WavetableLibrary& libraryToUse)
        : juce::Thread ("GNARL table loader"), library (libraryToUse)
    {
    }

    ~TableLoader() override
    {
        // 2 seconds: generating one table is tens of milliseconds, so this is
        // orders of magnitude more than a clean exit needs, and a hard kill
        // during an FFT would leak the half-built table.
        stopThread (2000);

        // AsyncUpdater cancels its own pending update on destruction, which is
        // the reason the completion notice goes through one rather than
        // through MessageManager::callAsync: a callAsync lambda capturing the
        // processor would still be sitting in the message queue when the
        // processor went away, and would run against freed memory. That is a
        // crash on closing a plugin window at the wrong moment - rare, and
        // impossible to reproduce on request.
        cancelPendingUpdate();
    }

    void setCallback (OnReady callback) { onReady = std::move (callback); }

    /** MESSAGE THREAD. Asks for these tables, superseding any outstanding
        request. Returns immediately.

        Tables that are already built are not queued: the common case - a
        preset selecting a table the user has already used - then costs
        nothing at all and the callback fires on the spot. */
    void request (const std::set<int>& indices)
    {
        std::set<int> missing;

        for (const auto index : indices)
            if (library.getTableIfLoaded (index) == nullptr)
                missing.insert (index);

        if (missing.empty())
        {
            if (onReady)
                onReady();

            return;
        }

        // Supersede rather than queue. Clicking down a bank of two hundred
        // presets must not generate two hundred tables in order; only the one
        // the user has landed on matters.
        stopThread (2000);

        {
            const juce::ScopedLock lock (requestLock);
            wanted = std::move (missing);
        }

        startThread (juce::Thread::Priority::low);
    }

    /** True while a generation pass is running, for the UI's "loading" mark. */
    bool isWorking() const noexcept { return working.load(); }

private:
    void run() override
    {
        working.store (true);

        std::set<int> toBuild;

        {
            const juce::ScopedLock lock (requestLock);
            toBuild = wanted;
        }

        for (const auto index : toBuild)
        {
            if (threadShouldExit())
                break;

            // getTable generates and caches. It allocates and runs FFTs, which
            // is exactly why this is not on the message thread and could never
            // be on the audio one.
            library.getTable (index);
        }

        working.store (false);

        if (threadShouldExit())
            return;

        // Back to the message thread to publish: the callback republishes the
        // pointers the audio thread reads, and doing that from here would be a
        // second writer to state the message thread owns.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        if (onReady)
            onReady();
    }

    dsp::WavetableLibrary& library;

    juce::CriticalSection requestLock;
    std::set<int> wanted;

    std::atomic<bool> working { false };
    OnReady onReady;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableLoader)
};

} // namespace gnarl::preset
