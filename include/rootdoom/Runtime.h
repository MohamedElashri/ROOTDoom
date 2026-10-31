// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>
#include <vector>

namespace rootdoom {
enum class RunMode { Baseline, NativePattern, BatchPattern, NativeWorker, BatchWorker, NativeGame, BatchGame };
enum class Renderer { Classic, RootGL };
struct RunOptions {
    RunMode mode = RunMode::NativeGame;
    Renderer renderer = Renderer::RootGL;
    bool wireframe = false;
    unsigned duration_seconds = 0;
    bool exercise_resize = false;
    bool log_input = false;
    std::string screenshot;
    std::string metrics;
    std::string iwad;
    std::string data_directory = "rootdoom-data";
    std::vector<std::string> doom_arguments;
};
int RunApplication(const RunOptions& options, char* executable);
} // namespace rootdoom
