// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>
#include <vector>

namespace rootdoom {
enum class DemoMode { Baseline, NativePattern, BatchPattern, NativeWorker, BatchWorker, NativeGame, BatchGame };
struct DemoOptions {
    DemoMode mode = DemoMode::NativeGame;
    unsigned duration_seconds = 0;
    bool exercise_resize = false;
    bool log_input = false;
    std::string screenshot;
    std::string metrics;
    std::string iwad;
    std::string data_directory = "rootdoom-data";
    std::vector<std::string> doom_arguments;
};
int RunDemo(const DemoOptions& options, char* executable);
} // namespace rootdoom
