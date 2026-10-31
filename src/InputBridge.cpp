// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/InputBridge.h"

namespace rootdoom {
void InputBridge::ReleaseNow(unsigned physical)
{
    const auto key = physical_[physical];
    if (key) {
        physical_[physical] = 0;
        if (--references_[key] == 0) queue_.SetKey(key, false);
    }
    deferred_[physical].active = false;
}
void InputBridge::Press(unsigned physical, unsigned char key, std::uint64_t event_time)
{
    if (physical >= physical_.size() || !key) return;
    auto& release = deferred_[physical];
    if (release.active) {
        if (event_time && release.event_time == event_time && physical_[physical] == key) {
            release.active = false; // X11 auto-repeat release/press pair.
            return;
        }
        ReleaseNow(physical);
    }
    if (physical_[physical] == key) return; // Repeated key-down without release.
    if (physical_[physical]) ReleaseNow(physical);
    physical_[physical] = key;
    if (++references_[key] == 1) queue_.SetKey(key, true);
}
void InputBridge::Release(unsigned physical, std::uint64_t event_time, std::uint64_t now_ms)
{
    if (physical >= physical_.size() || !physical_[physical]) return;
    // Allow the paired repeat press to arrive; a real release adds <=10 ms latency.
    deferred_[physical] = {true, event_time, now_ms + 10};
}
void InputBridge::Flush(std::uint64_t now_ms)
{
    for (unsigned i = 0; i < deferred_.size(); ++i) {
        if (deferred_[i].active && now_ms >= deferred_[i].deadline_ms) ReleaseNow(i);
    }
}
void InputBridge::ReleaseAll()
{
    physical_.fill(0);
    references_.fill(0);
    deferred_.fill({});
    queue_.ReleaseAll();
}
} // namespace rootdoom
