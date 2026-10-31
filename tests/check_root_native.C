// SPDX-License-Identifier: GPL-2.0-or-later
#include <TApplication.h>
#include <TROOT.h>
#include <TSystem.h>
#include <chrono>
#include <iostream>

void check_root_native(const char* library, const char* iwad, const char* renderer = nullptr)
{
    auto fail = [] { std::cerr << "ROOT_NATIVE_ENTRY_FAILED\n"; gSystem->Exit(1); };
    if (gSystem->Load(library) < 0) { fail(); return; }
    using Launch = int (*)(const char*, const char*, const char*);
    using DefaultLaunch = int (*)(const char*, const char*);
    using Running = int (*)();
    auto entry = reinterpret_cast<Launch>(gSystem->DynFindSymbol(library, "RootDoomLaunchRenderer"));
    auto default_entry = reinterpret_cast<DefaultLaunch>(gSystem->DynFindSymbol(library, "RootDoomLaunch"));
    const std::string data = std::string("root-native-") + (renderer ? renderer : "default") + "-test-data";
    auto launch = [&] { return renderer ? entry(iwad, data.c_str(), renderer) : default_entry(iwad, data.c_str()); };
    auto running = reinterpret_cast<Running>(gSystem->DynFindSymbol(library, "RootDoomIsRunning"));
    if (!entry || !default_entry || !running || launch() != 0) { fail(); return; }
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < end) { gSystem->ProcessEvents(); gSystem->Sleep(5); }
    if (!running()) { fail(); return; }
    // Exercise the application's pre-teardown signal while the game is alive.
    gApplication->Emit("Terminate(Int_t)", 0);
    if (running() || gROOT->ProcessLine("1+1") != 2) { fail(); return; }
    if (launch() != 1) { fail(); return; }
    std::cout << "ROOT_NATIVE_ENTRY_OK: game stopped, host interpreter alive, relaunch guarded\n";
    gSystem->Exit(0);
}
