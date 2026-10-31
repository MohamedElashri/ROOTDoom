// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/DoomEngine.h"
#include "rootdoom/DoomRunner.h"
#include "rootdoom/Wad.h"
#include "doomkeys.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
void Tap(rootdoom::InputQueue& input, unsigned char key)
{
    input.SetKey(key, true); std::this_thread::sleep_for(100ms);
    input.SetKey(key, false); std::this_thread::sleep_for(100ms);
}
} // namespace
int main(int argc, char** argv)
{
    try {
        Require(argc == 3, "Expected IWAD path and movement/quit scenario");
        const auto wad = rootdoom::ValidateWad(argv[1]);
        const bool quit = std::string(argv[2]) == "quit";
        rootdoom::FrameMailbox frames;
        rootdoom::InputQueue input;
        rootdoom::EngineReport report;
        rootdoom::EngineOptions options{wad.path, quit ? "game-quit-data" : "game-input-data",
                                       {"-warp", "1", "1", "-nomonsters"}, false};
        rootdoom::DoomRunner runner;
        runner.Start([&](rootdoom::Clock& clock) {
            rootdoom::RunDoom(clock, frames, input, options, report, [](const std::string&) {});
        });
        const auto ready_deadline = std::chrono::steady_clock::now() + 5s;
        while (!report.initialized.load() && runner.State() == rootdoom::WorkerState::Running &&
               std::chrono::steady_clock::now() < ready_deadline) std::this_thread::sleep_for(5ms);
        Require(report.initialized.load() && frames.Counts().published > 0, "Engine initialization did not finish");
        if (quit) {
            Tap(input, KEY_F10);
            Tap(input, 'y');
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (runner.State() == rootdoom::WorkerState::Running && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(10ms);
            }
            Require(runner.State() == rootdoom::WorkerState::Finished, "Doom menu quit did not finish the worker");
        } else {
            input.SetKey(KEY_UPARROW, true); std::this_thread::sleep_for(400ms); input.SetKey(KEY_UPARROW, false);
            input.SetKey(KEY_LEFTARROW, true); std::this_thread::sleep_for(300ms); input.SetKey(KEY_LEFTARROW, false);
            input.SetKey(KEY_FIRE, true); std::this_thread::sleep_for(650ms); input.SetKey(KEY_FIRE, false);
            Tap(input, KEY_USE);
        }
        runner.RequestStop();
        runner.Join();
        Require(runner.State() == rootdoom::WorkerState::Finished, "Real engine failed/stayed alive");
        Require(report.initial_player.valid && report.final_player.valid, "Real level state was not observed");
        if (!quit) {
            Require(report.initial_player.x != report.final_player.x || report.initial_player.y != report.final_player.y, "Movement had no gameplay effect");
            Require(report.initial_player.angle != report.final_player.angle, "Turning had no gameplay effect");
            Require(report.final_player.bullets < report.initial_player.bullets, "Firing did not consume ammunition");
            Require(report.use_presses > 0, "Use input did not reach Doom");
        }
        Require(report.allocations_after_cleanup == 0 && report.files_after_cleanup == 0, "Engine owned resources survived exit");
        std::cout << "GAMEPLAY_OK " << argv[2] << ": live E1M1, host survived, owned resources cleaned\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
