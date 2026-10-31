// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/InputQueue.h"

namespace rootdoom {
// GUI-thread-only physical key tracking. Both Ctrl/Shift sides share Doom keys.
class InputBridge {
public:
    explicit InputBridge(InputQueue& queue) : queue_(queue) {}
    void Press(unsigned physical, unsigned char key, std::uint64_t event_time);
    void Release(unsigned physical, std::uint64_t event_time, std::uint64_t now_ms);
    void Flush(std::uint64_t now_ms);
    void ReleaseAll();
private:
    void ReleaseNow(unsigned physical);
    struct Deferred { bool active = false; std::uint64_t event_time = 0, deadline_ms = 0; };
    InputQueue& queue_;
    std::array<unsigned char, 256> physical_{};
    std::array<unsigned, 256> references_{};
    std::array<Deferred, 256> deferred_{};
};
} // namespace rootdoom
