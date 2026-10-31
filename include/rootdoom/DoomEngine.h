// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/Clock.h"
#include "rootdoom/FrameMailbox.h"
#include "rootdoom/InputQueue.h"
#include <functional>
#include <string>

namespace rootdoom {
class GeometryMailbox;
struct EngineOptions {
    std::string iwad;
    std::string data_directory;
    std::vector<std::string> arguments;
    bool log_input = false;
    GeometryMailbox* geometry = nullptr; // Optional immutable scene publisher, outlives worker.
};
struct PlayerSnapshot {
    bool valid = false;
    std::int32_t x = 0, y = 0;
    std::uint32_t angle = 0;
    int bullets = 0, health = 0, map = 0;
};
// Written only by worker; read only after joining that worker.
struct EngineReport {
    std::atomic<bool> initialized{false}; // The one field readable while worker runs.
    std::size_t peak_allocations = 0, peak_bytes = 0;
    std::size_t allocations_after_cleanup = 0, files_after_cleanup = 0;
    PlayerSnapshot initial_player, final_player;
    std::uint64_t use_presses = 0;
    std::uint64_t software_views_skipped = 0;
};
void RunDoom(Clock& clock, FrameMailbox& frames, InputQueue& input,
             const EngineOptions& options, EngineReport& report,
             const std::function<void(const std::string&)>& title);
bool DoomHasRun();
} // namespace rootdoom
