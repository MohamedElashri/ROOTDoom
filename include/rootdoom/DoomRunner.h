// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/Clock.h"
#include <functional>
#include <string>
#include <thread>

namespace rootdoom {
enum class WorkerState { Ready, Running, Finished, Failed };

// Single-start owner. Construct another runner for another synthetic session;
// the engine itself will enforce its separate single-instance process policy.
class DoomRunner {
public:
    ~DoomRunner();
    void Start(std::function<void(Clock&)> body);
    void RequestStop() { clock_.RequestStop(); }
    void Join(); // GUI/owner thread only; body must honor the stop clock.
    WorkerState State() const { return state_.load(); }
    std::string Error() const;
private:
    Clock clock_;
    std::thread thread_;
    std::atomic<WorkerState> state_{WorkerState::Ready};
    mutable std::mutex error_mutex_;
    std::string error_;
};
} // namespace rootdoom
