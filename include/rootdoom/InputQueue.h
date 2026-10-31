// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace rootdoom {
struct KeyEvent { unsigned char key = 0; bool pressed = false; };
struct InputCounts { std::uint64_t delivered = 0, overflows = 0; std::size_t queued = 0; };

// On overflow reconcile delivered state to desired state (at most 256 events).
// Fast taps may coalesce under saturation; a held-key release is never lost.
class InputQueue {
public:
    static constexpr std::size_t kCapacity = 256;
    void SetKey(unsigned char key, bool pressed);
    bool Pop(KeyEvent& event);
    void ReleaseAll();
    InputCounts Counts() const;
private:
    void Reconcile(); // Lock held.
    mutable std::mutex mutex_;
    std::array<KeyEvent, kCapacity> events_{};
    std::array<bool, 256> desired_{}, delivered_{};
    std::size_t head_ = 0, size_ = 0;
    std::uint64_t delivered_count_ = 0, overflows_ = 0;
};
} // namespace rootdoom
