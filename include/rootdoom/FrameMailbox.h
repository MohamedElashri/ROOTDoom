// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/Frame.h"
#include <mutex>

namespace rootdoom {
struct FrameCounts { std::uint64_t published = 0, consumed = 0, overwritten = 0; };

// One pending frame. Consume swaps into GUI-owned storage; no lock spans paint.
class FrameMailbox {
public:
    FrameMailbox();
    void Publish(const std::uint32_t* pixels, std::size_t count);
    bool Consume(std::vector<std::uint32_t>& destination, std::uint64_t& sequence);
    FrameCounts Counts() const;
private:
    mutable std::mutex mutex_;
    std::vector<std::uint32_t> pending_;
    bool ready_ = false;
    FrameCounts counts_;
};
} // namespace rootdoom
