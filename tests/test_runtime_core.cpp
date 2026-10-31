// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Clock.h"
#include "rootdoom/DoomRunner.h"
#include "rootdoom/FrameMailbox.h"
#include "rootdoom/InputBridge.h"
#include "rootdoom/KeyMapping.h"
#include "doomkeys.h"
#include <KeySymbols.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace rootdoom;
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

void CheckKeys()
{
    const std::array<std::pair<unsigned, unsigned char>, 19> cases{{
        {kKey_Left, KEY_LEFTARROW}, {kKey_Right, KEY_RIGHTARROW}, {kKey_Up, KEY_UPARROW},
        {kKey_Down, KEY_DOWNARROW}, {kKey_Escape, KEY_ESCAPE}, {kKey_Return, KEY_ENTER},
        {kKey_Enter, KEY_ENTER}, {kKey_Space, KEY_USE}, {kKey_Control, KEY_FIRE},
        {kKey_Shift, KEY_RSHIFT}, {kKey_Alt, KEY_RALT}, {kKey_Tab, KEY_TAB},
        {kKey_Backspace, KEY_BACKSPACE}, {kKey_F1, KEY_F1}, {kKey_F10, KEY_F10},
        {kKey_F12, KEY_F12}, {kKey_Plus, KEY_EQUALS}, {'A', 'a'}, {'9', '9'}
    }};
    for (const auto& pair : cases) Require(DoomKey(pair.first) == pair.second, "Key translation mismatch");
    Require(DoomKey(kKey_Unknown) == 0 && DoomKey(0x100000) == 0, "Unknown symbol accepted");

    InputQueue queue;
    InputBridge bridge(queue);
    KeyEvent event;
    bridge.Press(37, KEY_FIRE, 1);
    bridge.Press(105, KEY_FIRE, 2);
    Require(queue.Pop(event) && event.pressed && event.key == KEY_FIRE, "Ctrl press missing");
    Require(!queue.Pop(event), "Both modifier sides duplicated press");
    bridge.Release(37, 3, 3);
    bridge.Flush(13);
    Require(!queue.Pop(event), "Released one Ctrl while the other remains held");
    bridge.Release(105, 14, 14);
    bridge.Flush(23);
    Require(!queue.Pop(event), "Release did not wait for repeat coalescing");
    bridge.Flush(24);
    Require(queue.Pop(event) && !event.pressed, "Final modifier release missing");

    bridge.Press(38, 'a', 100);
    Require(queue.Pop(event) && event.pressed, "Initial press missing");
    for (unsigned i = 101; i < 2101; ++i) {
        bridge.Press(38, 'a', i); // Repeated-down platforms.
        bridge.Release(38, i, i);
        bridge.Press(38, 'a', i); // X11 paired auto-repeat.
        bridge.Flush(i + 10);
    }
    Require(!queue.Pop(event), "Auto-repeat created spurious edges");
    bridge.Release(38, 3000, 3000);
    bridge.Flush(3010);
    Require(queue.Pop(event) && !event.pressed, "Held-key release lost after repeat");
    bridge.Press(38, 'a', 4000);
    bridge.Release(38, 4001, 4001);
    bridge.Press(38, 'a', 4002);
    bridge.Release(38, 4003, 4003);
    bridge.Flush(4013);
    for (bool expected : {true, false, true, false}) {
        Require(queue.Pop(event) && event.pressed == expected, "A real rapid tap was coalesced");
    }
    bridge.Press(38, 'a', 5000);
    Require(queue.Pop(event) && event.pressed, "Focus test press missing");
    bridge.ReleaseAll();
    Require(queue.Pop(event) && !event.pressed && !queue.Pop(event), "Focus loss stuck a key");
}

void CheckInputOverflow()
{
    InputQueue queue;
    std::array<bool, 256> state{};
    KeyEvent event;
    queue.SetKey('a', true);
    Require(queue.Pop(event), "Initial event missing");
    state[event.key] = event.pressed;
    for (int i = 0; i < 4000; ++i) queue.SetKey('b', i % 2 == 0);
    queue.SetKey('a', false);
    Require(queue.Counts().overflows > 0 && queue.Counts().queued <= InputQueue::kCapacity, "Overflow policy not exercised");
    queue.ReleaseAll();
    while (queue.Pop(event)) state[event.key] = event.pressed;
    Require(std::none_of(state.begin(), state.end(), [](bool x) { return x; }), "Overflow lost a key release");

    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (unsigned i = 0; i < 100000; ++i) queue.SetKey(static_cast<unsigned char>(1 + i % 255), (i / 255) % 2);
        queue.ReleaseAll();
        done.store(true);
    });
    while (!done.load() || queue.Counts().queued) {
        if (queue.Pop(event)) state[event.key] = event.pressed;
        else std::this_thread::yield();
    }
    producer.join();
    Require(std::none_of(state.begin(), state.end(), [](bool x) { return x; }), "Concurrent input state stuck");
}

void CheckMailbox()
{
    FrameMailbox mailbox;
    std::atomic<bool> done{false};
    constexpr unsigned frames = 1000;
    std::thread producer([&] {
        std::vector<std::uint32_t> frame(kFramePixels);
        for (unsigned i = 1; i <= frames; ++i) {
            std::fill(frame.begin(), frame.end(), i);
            mailbox.Publish(frame.data(), frame.size());
        }
        done.store(true);
    });
    std::vector<std::uint32_t> staging;
    std::uint64_t sequence = 0, previous = 0;
    bool consistent = true;
    while (!done.load() || sequence < frames) {
        if (mailbox.Consume(staging, sequence)) {
            consistent = consistent && sequence > previous &&
                std::all_of(staging.begin(), staging.end(), [sequence](auto value) { return value == sequence; });
            previous = sequence;
        } else std::this_thread::yield();
    }
    producer.join();
    const auto counts = mailbox.Counts();
    Require(consistent && sequence == frames, "Mailbox tearing or sequence regression");
    Require(counts.published == frames && counts.consumed + counts.overwritten == frames, "Mailbox accounting mismatch");
    Require(!mailbox.Consume(staging, sequence), "Consumed frame remained pending");
    bool rejected = false;
    try { mailbox.Publish(nullptr, 0); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "Invalid mailbox frame accepted");
}

void CheckLifecycle()
{
    Clock clock;
    const auto t0 = clock.TicksMs();
    Require(t0 >= 1 && clock.SleepMs(2) && clock.TicksMs() >= t0 + 1, "Non-monotonic clock/sleep");
    clock.RequestStop();
    Require(!clock.SleepMs(5000), "Cancelled wait blocked");
    for (int i = 0; i < 50; ++i) {
        DoomRunner runner;
        const auto start = std::chrono::steady_clock::now();
        runner.Start([](Clock& worker) { while (!worker.StopRequested()) worker.SleepMs(5000); });
        runner.RequestStop();
        runner.Join();
        Require(runner.State() == WorkerState::Finished, "Stop-before-first-frame failed");
        Require(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500), "Worker join blocked on sleep");
    }
    FrameMailbox mailbox;
    DoomRunner runner;
    runner.Start([&](Clock& worker) {
        std::vector<std::uint32_t> frame(kFramePixels, 42);
        while (!worker.StopRequested()) {
            mailbox.Publish(frame.data(), frame.size());
            worker.SleepMs(1);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    runner.RequestStop();
    runner.Join();
    Require(mailbox.Counts().published > 0 && runner.State() == WorkerState::Finished, "Close-under-load failed");
    DoomRunner failure;
    failure.Start([](Clock&) { throw std::runtime_error("startup failed"); });
    failure.Join();
    Require(failure.State() == WorkerState::Failed && failure.Error() == "startup failed", "Worker error not propagated");
    bool rejected = false;
    try { failure.Start([](Clock&) {}); } catch (const std::logic_error&) { rejected = true; }
    Require(rejected, "Duplicate worker start accepted");
}
} // namespace

int main()
{
    try {
        CheckKeys(); CheckInputOverflow(); CheckMailbox(); CheckLifecycle();
        std::cout << "Runtime core: mapping, repeat/focus/modifiers, overflow, concurrent complete frames, clock and lifecycle passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Runtime core failed: " << error.what() << '\n';
        return 1;
    }
}
