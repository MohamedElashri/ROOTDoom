// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Clock.h"

namespace rootdoom {
std::uint32_t Clock::TicksMs() const
{
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - epoch_).count();
    // Start at 1 because Doom uses 0 as an uninitialized timer-base sentinel.
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(ms) + 1);
}
bool Clock::SleepMs(std::uint32_t ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return !wake_.wait_for(lock, std::chrono::milliseconds(ms), [this] { return stop_.load(); });
}
void Clock::RequestStop()
{
    { std::lock_guard<std::mutex> lock(mutex_); stop_.store(true); }
    wake_.notify_all();
}
} // namespace rootdoom
