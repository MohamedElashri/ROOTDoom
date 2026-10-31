// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/DoomEngine.h"
#include "rootdoom/DoomRunner.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
} // namespace
int main(int argc, char** argv)
{
    try {
        const std::string mode = argc > 1 ? argv[1] : "failure";
        const std::array<unsigned char, 28> fixture{'I','W','A','D',1,0,0,0,12,0,0,0,
            0,0,0,0,0,0,0,0,'E','1','M','1',0,0,0,0};
        {
            std::ofstream file("engine-incomplete-test.wad", std::ios::binary);
            file.write(reinterpret_cast<const char*>(fixture.data()), fixture.size());
        }
        rootdoom::FrameMailbox frames;
        rootdoom::InputQueue input;
        rootdoom::EngineReport report;
        rootdoom::EngineOptions options{std::filesystem::absolute("engine-incomplete-test.wad").string(), "engine-test-data", {}, false};
        rootdoom::DoomRunner runner;
        if (mode != "failure") runner.RequestStop();
        runner.Start([&](rootdoom::Clock& clock) { rootdoom::RunDoom(clock, frames, input, options, report, [](const std::string&) {}); });
        runner.Join();
        if (mode == "failure") {
            Require(runner.State() == rootdoom::WorkerState::Failed && !runner.Error().empty(), "Engine startup error did not reach host");
            Require(report.peak_bytes > 0, "Engine C startup was not exercised");
        } else {
            Require(runner.State() == rootdoom::WorkerState::Finished && frames.Counts().published == 0, "Stop before first frame failed");
            rootdoom::Clock next;
            bool rejected = false;
            try { rootdoom::RunDoom(next, frames, input, options, report, [](const std::string&) {}); }
            catch (const std::logic_error&) { rejected = true; }
            Require(rejected, "A second engine session bypassed the process guard");
        }
        Require(report.allocations_after_cleanup == 0 && report.files_after_cleanup == 0, "Engine left owned resources alive");
        std::filesystem::remove("engine-incomplete-test.wad");
        std::cout << "ENGINE_HOST_SURVIVED: " << mode << ", owned allocations and files cleaned\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
