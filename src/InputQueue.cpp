// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/InputQueue.h"

namespace rootdoom {
void InputQueue::Reconcile()
{
    head_ = size_ = 0;
    for (unsigned key = 1; key < desired_.size(); ++key) {
        if (desired_[key] != delivered_[key]) {
            events_[size_++] = {static_cast<unsigned char>(key), desired_[key]};
        }
    }
}
void InputQueue::SetKey(unsigned char key, bool pressed)
{
    if (!key) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (desired_[key] == pressed) return;
    desired_[key] = pressed;
    if (size_ == kCapacity) {
        ++overflows_;
        Reconcile();
    } else {
        events_[(head_ + size_) % kCapacity] = {key, pressed};
        ++size_;
    }
}
bool InputQueue::Pop(KeyEvent& event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!size_) return false;
    event = events_[head_];
    head_ = (head_ + 1) % kCapacity;
    --size_;
    delivered_[event.key] = event.pressed;
    ++delivered_count_;
    return true;
}
void InputQueue::ReleaseAll()
{
    std::lock_guard<std::mutex> lock(mutex_);
    desired_.fill(false);
    Reconcile();
}
InputCounts InputQueue::Counts() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {delivered_count_, overflows_, size_};
}
} // namespace rootdoom
