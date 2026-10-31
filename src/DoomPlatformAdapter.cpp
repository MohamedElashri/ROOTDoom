// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/DoomEngine.h"
#include "rootdoom/EngineHooks.h"

extern "C" {
#include "doomgeneric.h"
#include "doomstat.h"
#include "doomkeys.h"
}

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
struct EngineExit { int status; };
struct Context;
thread_local Context* active = nullptr;
std::atomic<bool> engine_used{false};

struct Context {
    rootdoom::Clock& clock;
    rootdoom::FrameMailbox& frames;
    rootdoom::InputQueue& input;
    const rootdoom::EngineOptions& options;
    rootdoom::EngineReport& report;
    const std::function<void(const std::string&)>& title;
    std::unordered_map<void*, std::size_t> allocations;
    std::unordered_set<FILE*> files;
    std::size_t bytes = 0;
    std::string error, state_directory;

    Context(rootdoom::Clock& c, rootdoom::FrameMailbox& f, rootdoom::InputQueue& i,
            const rootdoom::EngineOptions& o, rootdoom::EngineReport& r,
            const std::function<void(const std::string&)>& t)
        : clock(c), frames(f), input(i), options(o), report(r), title(t)
    {
        const auto path = std::filesystem::absolute(options.data_directory).lexically_normal();
        std::filesystem::create_directories(path);
        state_directory = path.string() + std::filesystem::path::preferred_separator;
        active = this;
    }
    ~Context()
    {
        for (FILE* file : files) std::fclose(file);
        files.clear();
        for (const auto& allocation : allocations) std::free(allocation.first);
        allocations.clear();
        DG_ScreenBuffer = nullptr;
        report.allocations_after_cleanup = allocations.size();
        report.files_after_cleanup = files.size();
        active = nullptr;
    }
    void Remember(void* pointer, std::size_t size)
    {
        if (!pointer) { if (size) throw std::bad_alloc(); return; }
        try { allocations.emplace(pointer, size); }
        catch (...) { std::free(pointer); throw; }
        bytes += size;
        report.peak_bytes = std::max(report.peak_bytes, bytes);
        report.peak_allocations = std::max(report.peak_allocations, allocations.size());
    }
};
Context& Current()
{
    if (!active) throw std::logic_error("Doom platform callback outside its worker lifetime");
    return *active;
}
void CheckStop() { if (Current().clock.StopRequested()) throw EngineExit{0}; }
void SamplePlayer(rootdoom::EngineReport& report)
{
    if (gamestate != GS_LEVEL || !players[consoleplayer].mo) return;
    const auto& player = players[consoleplayer];
    const rootdoom::PlayerSnapshot snapshot{true, player.mo->x, player.mo->y, player.mo->angle,
                                           player.ammo[am_clip], player.health, gamemap};
    if (!report.initial_player.valid) report.initial_player = snapshot;
    report.final_player = snapshot;
}
} // namespace

extern "C" void RootDoom_Exit(int status) { throw EngineExit{status}; }
extern "C" void RootDoom_ReportError(const char* format, va_list args)
{
    char message[2048]{};
    std::vsnprintf(message, sizeof(message), format, args);
    Current().error = message;
    std::cerr << "Doom: " << message << '\n';
}
extern "C" char* RootDoom_StateDirectory() { return Current().state_directory.data(); }
extern "C" void* RootDoom_Malloc(std::size_t size)
{
    void* pointer = std::malloc(size);
    Current().Remember(pointer, size);
    return pointer;
}
extern "C" void* RootDoom_Calloc(std::size_t count, std::size_t size)
{
    if (size && count > std::numeric_limits<std::size_t>::max() / size) throw std::bad_alloc();
    void* pointer = std::calloc(count, size);
    Current().Remember(pointer, count * size);
    return pointer;
}
extern "C" void RootDoom_Free(void* pointer)
{
    auto& context = Current();
    const auto found = context.allocations.find(pointer);
    if (found != context.allocations.end()) {
        context.bytes -= found->second;
        context.allocations.erase(found);
    }
    std::free(pointer);
}
extern "C" void* RootDoom_Realloc(void* pointer, std::size_t size)
{
    if (!pointer) return RootDoom_Malloc(size);
    if (!size) { RootDoom_Free(pointer); return nullptr; }
    auto& context = Current();
    const auto found = context.allocations.find(pointer);
    if (found == context.allocations.end()) throw std::logic_error("Unowned Doom realloc");
    const auto old_size = found->second;
    // Extracting the node avoids an allocation while updating a successful realloc.
    auto node = context.allocations.extract(found);
    void* resized = std::realloc(pointer, size);
    if (!resized) { context.allocations.insert(std::move(node)); throw std::bad_alloc(); }
    node.key() = resized;
    node.mapped() = size;
    context.allocations.insert(std::move(node));
    context.bytes = context.bytes - old_size + size;
    context.report.peak_bytes = std::max(context.report.peak_bytes, context.bytes);
    return resized;
}
extern "C" char* RootDoom_Strdup(const char* text)
{
    const auto size = std::strlen(text) + 1;
    auto* copy = static_cast<char*>(RootDoom_Malloc(size));
    std::memcpy(copy, text, size);
    return copy;
}
extern "C" FILE* RootDoom_Fopen(const char* path, const char* mode)
{
    FILE* file = std::fopen(path, mode);
    if (file) {
        try { Current().files.insert(file); }
        catch (...) { std::fclose(file); throw; }
    }
    return file;
}
extern "C" int RootDoom_Fclose(FILE* file)
{
    Current().files.erase(file);
    return std::fclose(file);
}

extern "C" void DG_Init() { CheckStop(); }
extern "C" void DG_DrawFrame()
{
    CheckStop();
    static_assert(DOOMGENERIC_RESX == rootdoom::kFrameWidth && DOOMGENERIC_RESY == rootdoom::kFrameHeight);
    Current().frames.Publish(DG_ScreenBuffer, rootdoom::kFramePixels);
}
extern "C" void DG_SleepMs(std::uint32_t ms)
{
    CheckStop();
    Current().clock.SleepMs(ms);
    CheckStop();
}
extern "C" std::uint32_t DG_GetTicksMs() { CheckStop(); return Current().clock.TicksMs(); }
extern "C" int DG_GetKey(int* pressed, unsigned char* key)
{
    CheckStop();
    rootdoom::KeyEvent event;
    if (!Current().input.Pop(event)) return 0;
    *pressed = event.pressed ? 1 : 0;
    *key = event.key;
    if (event.pressed && event.key == KEY_USE) ++Current().report.use_presses;
    if (Current().options.log_input) std::cout << "[input] key=" << static_cast<unsigned>(event.key)
                                            << (event.pressed ? " down" : " up") << std::endl;
    return 1;
}
extern "C" void DG_SetWindowTitle(const char* title)
{
    CheckStop();
    Current().title(title ? std::string(title).substr(0, 256) : "ROOTDoom");
}

namespace rootdoom {
bool DoomHasRun() { return engine_used.load(); }
void RunDoom(Clock& clock, FrameMailbox& frames, InputQueue& input, const EngineOptions& options,
             EngineReport& report, const std::function<void(const std::string&)>& title)
{
    if (engine_used.exchange(true)) throw std::logic_error("Doom can run only once per process; restart ROOT for a new session");
    Context context(clock, frames, input, options, report, title);
    std::vector<std::string> arguments{"rootdoom", "-iwad", options.iwad, "-nogui", "-nosound", "-noautoload"};
    arguments.insert(arguments.end(), options.arguments.begin(), options.arguments.end());
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    try {
        CheckStop();
        doomgeneric_Create(static_cast<int>(arguments.size()), argv.data());
        SamplePlayer(report);
        report.initialized.store(true);
        while (!clock.StopRequested()) {
            doomgeneric_Tick();
            SamplePlayer(report);
        }
    } catch (const EngineExit& exit) {
        if (exit.status != 0 || !context.error.empty()) {
            throw std::runtime_error(context.error.empty() ? "Doom exited with status " + std::to_string(exit.status) : context.error);
        }
    }
}
} // namespace rootdoom
