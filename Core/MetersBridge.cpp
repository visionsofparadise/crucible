#include "MetersBridge.h"

namespace crucible {

MetersBridge::MetersBridge()
    : fifo (kCapacity)
{
}

bool MetersBridge::tryPush (const MeterFrame& frame) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 < 1)
        return false; // Ring full — drop. UI is responsible for keeping up.

    if (size1 > 0)
        ring[(size_t) start1] = frame;
    else
        ring[(size_t) start2] = frame;

    fifo.finishedWrite (1);
    return true;
}

bool MetersBridge::tryPopLatest (MeterFrame& out) noexcept
{
    const int available = fifo.getNumReady();
    if (available <= 0)
        return false;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (available, start1, size1, start2, size2);

    // Latest = the last frame across the (possibly wrapping) read window.
    // The two blocks are contiguous in producer order: block1 first,
    // then block2 (if the read wraps). The newest sample is always at
    // the end of block2 (when non-empty), otherwise at the end of block1.
    if (size2 > 0)
        out = ring[(size_t) (start2 + size2 - 1)];
    else
        out = ring[(size_t) (start1 + size1 - 1)];

    fifo.finishedRead (size1 + size2);
    return true;
}

} // namespace crucible
