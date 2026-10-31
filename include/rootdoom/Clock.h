// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace rootdoom {
class Clock {
public:
    std::uint32_t TicksMs() const; // Unsigned milliseconds modulo 2^32 (~49.7 days).
    bool SleepMs(std::uint32_t ms); // False if cancelled, including before wait.
    void RequestStop();
    bool StopRequested() const { return stop_.load(); }
private:
    const std::chrono::steady_clock::time_point epoch_ = std::chrono::steady_clock::now();
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    std::condition_variable wake_;
};
} // namespace rootdoom
