// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/FrameMailbox.h"
#include <algorithm>
#include <stdexcept>

namespace rootdoom {
FrameMailbox::FrameMailbox() : pending_(kFramePixels) {}
void FrameMailbox::Publish(const std::uint32_t* pixels, std::size_t count)
{
    if (!pixels || count != kFramePixels) throw std::invalid_argument("Invalid mailbox framebuffer");
    std::lock_guard<std::mutex> lock(mutex_);
    std::copy_n(pixels, count, pending_.begin());
    if (ready_) ++counts_.overwritten;
    ++counts_.published;
    ready_ = true;
}
bool FrameMailbox::Consume(std::vector<std::uint32_t>& destination, std::uint64_t& sequence)
{
    // Allocate outside the lock; after the first call both buffers keep capacity.
    if (destination.size() != kFramePixels) destination.resize(kFramePixels);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ready_) return false;
    pending_.swap(destination);
    sequence = counts_.published;
    ready_ = false;
    ++counts_.consumed;
    return true;
}
FrameCounts FrameMailbox::Counts() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return counts_;
}
} // namespace rootdoom
