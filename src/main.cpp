// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Runtime.h"
#include "rootdoom/Wad.h"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void Usage()
{
    std::cout << "ROOTDoom: Doom inside a native CERN ROOT canvas (silent)\n"
                 "Usage: rootdoom [MODE] [OPTIONS]\n"
                 "  --iwad WAD          Required Doom IWAD (for example doom1.wad).\n"
                 "  --renderer MODE     rootgl (default) or classic (software framebuffer).\n"
                 "  --wireframe         Draw the rootgl world as triangle edges.\n"
                 "  --batch-game        Real engine/presenter smoke without a display.\n"
                 "  --data-dir DIR      Configuration/save directory (default: ./rootdoom-data).\n"
                 "  --self-test-gui     Black ROOT canvas; close after one second.\n"
                 "  --test-pattern      Animate 640 x 400 RGB patterns in a native ROOT canvas.\n"
                 "  --batch-pattern     Same presenter without a display; requires --duration.\n"
                 "  --worker-pattern    Threaded pattern with keyboard/focus input visualizer.\n"
                 "  --batch-worker      Threaded presenter test without a display.\n"
                 "  --log-input         Log key transitions consumed by the worker.\n"
                 "  --duration SECONDS  Stop after 1..600 seconds.\n"
                 "  --exercise-resize   Cycle wide/square/tall sizes every ten seconds.\n"
                 "  --screenshot PNG    Capture final canvas after stopping the timer.\n"
                 "  --metrics JSON      Save FPS, timing, CPU and memory measurements.\n"
                 "  --help              Show usage. Default mode is native Doom.\n"
                 "  -- DOOM_ARGS        Allowlist: -warp, -skill, -nomonsters, -fast, -respawn, -devparm.\n"
                 "Native patterns use a 30 FPS presentation target. Batch FPS is not a native benchmark.\n"
                 "Controls: arrows move/turn, Ctrl fire, Space use, Shift run, Alt strafe, Enter select, Esc menu.\n"
                 "One Doom session per process; restart ROOT to launch again.\n";
}

} // namespace

int main(int argc, char** argv)
{
    rootdoom::RunOptions options;
    try {
        unsigned modes = 0;
        bool renderer_selected = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            auto value = [&]() -> std::string_view {
                if (i + 1 >= argc || std::string_view(argv[i + 1]).substr(0, 2) == "--") {
                    throw std::invalid_argument("Missing value for " + std::string(arg));
                }
                return argv[++i];
            };
            if (arg == "--help" || arg == "-h") {
                Usage();
                return 0;
            } else if (arg == "--self-test-gui") {
                ++modes;
                options.mode = rootdoom::RunMode::Baseline;
                options.duration_seconds = 1;
            } else if (arg == "--iwad") {
                options.iwad = value();
            } else if (arg == "--renderer") {
                renderer_selected = true;
                const auto renderer = value();
                if (renderer == "classic") options.renderer = rootdoom::Renderer::Classic;
                else if (renderer == "rootgl") options.renderer = rootdoom::Renderer::RootGL;
                else throw std::invalid_argument("--renderer must be classic or rootgl");
            } else if (arg == "--wireframe") {
                options.wireframe = true;
            } else if (arg == "--batch-game") {
                ++modes;
                options.mode = rootdoom::RunMode::BatchGame;
            } else if (arg == "--data-dir") {
                options.data_directory = value();
            } else if (arg == "--test-pattern" || arg == "--batch-pattern") {
                ++modes;
                options.mode = arg == "--test-pattern" ? rootdoom::RunMode::NativePattern : rootdoom::RunMode::BatchPattern;
            } else if (arg == "--worker-pattern" || arg == "--batch-worker") {
                ++modes;
                options.mode = arg == "--worker-pattern" ? rootdoom::RunMode::NativeWorker : rootdoom::RunMode::BatchWorker;
            } else if (arg == "--duration") {
                const auto text = value();
                unsigned duration = 0;
                const auto parsed = std::from_chars(text.data(), text.data() + text.size(), duration);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || duration < 1 || duration > 600) {
                    throw std::invalid_argument("--duration must be an integer from 1 to 600");
                }
                options.duration_seconds = duration;
            } else if (arg == "--screenshot") {
                options.screenshot = value();
                if (std::filesystem::path(options.screenshot).extension() != ".png") {
                    throw std::invalid_argument("--screenshot requires a .png path");
                }
            } else if (arg == "--metrics") {
                options.metrics = value();
            } else if (arg == "--exercise-resize") {
                options.exercise_resize = true;
            } else if (arg == "--log-input") {
                options.log_input = true;
            } else if (arg == "--") {
                for (++i; i < argc; ++i) options.doom_arguments.emplace_back(argv[i]);
                break;
            } else {
                throw std::invalid_argument("Unknown or unavailable option: " + std::string(arg));
            }
        }
        if (modes > 1) throw std::invalid_argument("Choose only one display mode");
        const bool game = options.mode == rootdoom::RunMode::NativeGame || options.mode == rootdoom::RunMode::BatchGame;
        // Diagnostics and display-free modes retain the image presenter.
        if (!renderer_selected && options.mode != rootdoom::RunMode::NativeGame) options.renderer = rootdoom::Renderer::Classic;
        if (options.renderer == rootdoom::Renderer::RootGL && options.mode != rootdoom::RunMode::NativeGame) {
            throw std::invalid_argument("The rootgl renderer requires native game mode and an OpenGL display");
        }
        if (options.wireframe && options.renderer != rootdoom::Renderer::RootGL) throw std::invalid_argument("--wireframe requires --renderer rootgl");
        if ((options.mode == rootdoom::RunMode::BatchPattern || options.mode == rootdoom::RunMode::BatchWorker ||
             options.mode == rootdoom::RunMode::BatchGame) && !options.duration_seconds) {
            throw std::invalid_argument("Batch modes require --duration");
        }
        if (options.log_input && !game && options.mode != rootdoom::RunMode::NativeWorker && options.mode != rootdoom::RunMode::BatchWorker) {
            throw std::invalid_argument("--log-input requires a worker mode");
        }
        if (game) {
            options.iwad = rootdoom::ValidateWad(options.iwad).path;
            rootdoom::ValidateDoomArguments(options.doom_arguments);
        } else if (!options.iwad.empty() || !options.doom_arguments.empty()) {
            throw std::invalid_argument("IWAD/Doom arguments require a game mode");
        }
        if (options.mode == rootdoom::RunMode::Baseline &&
            (!options.screenshot.empty() || !options.metrics.empty() || options.exercise_resize)) {
            throw std::invalid_argument("Screenshot/metrics/resize options require a pattern mode");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << ". Use --help.\n";
        return 2;
    }
#if defined(__linux__)
    const char* display = std::getenv("DISPLAY");
    if (options.mode != rootdoom::RunMode::BatchPattern && options.mode != rootdoom::RunMode::BatchWorker &&
        options.mode != rootdoom::RunMode::BatchGame &&
        (display == nullptr || *display == '\0')) {
        std::cerr << "ROOTDoom requires a native X11 display. Set DISPLAY to an accessible X server.\n";
        return 1;
    }
#endif

    try {
        return rootdoom::RunApplication(options, argv[0]);
    } catch (const std::exception& error) {
        std::cerr << "ROOTDoom: " << error.what() << '\n';
        return 1;
    }
}
