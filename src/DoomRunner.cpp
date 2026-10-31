// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/DoomRunner.h"
#include <stdexcept>

namespace rootdoom {
DoomRunner::~DoomRunner() { RequestStop(); Join(); }
void DoomRunner::Start(std::function<void(Clock&)> body)
{
    WorkerState expected = WorkerState::Ready;
    if (!state_.compare_exchange_strong(expected, WorkerState::Running)) {
        throw std::logic_error("DoomRunner may only start once");
    }
    try {
        thread_ = std::thread([this, body = std::move(body)] {
            try {
                body(clock_);
                state_.store(WorkerState::Finished);
            } catch (const std::exception& error) {
                { std::lock_guard<std::mutex> lock(error_mutex_); error_ = error.what(); }
                state_.store(WorkerState::Failed);
            } catch (...) {
                { std::lock_guard<std::mutex> lock(error_mutex_); error_ = "Unknown worker failure"; }
                state_.store(WorkerState::Failed);
            }
        });
    } catch (...) {
        state_.store(WorkerState::Failed);
        throw;
    }
}
void DoomRunner::Join() { if (thread_.joinable()) thread_.join(); }
std::string DoomRunner::Error() const
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_;
}
} // namespace rootdoom
