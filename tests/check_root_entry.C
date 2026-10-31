// SPDX-License-Identifier: GPL-2.0-or-later
#include <TApplication.h>
#include <TSystem.h>
#include <iostream>

void check_root_entry(const char* library)
{
    auto fail = [] { std::cerr << "ROOT_ENTRY_FAILED\n"; gSystem->Exit(1); };
    if (gSystem->Load(library) < 0) { fail(); return; }
    using Launch = int (*)(const char*, const char*);
    using Running = int (*)();
    using Shutdown = void (*)();
    using LaunchRenderer = int (*)(const char*, const char*, const char*);
    auto launch = reinterpret_cast<Launch>(gSystem->DynFindSymbol(library, "RootDoomLaunch"));
    auto running = reinterpret_cast<Running>(gSystem->DynFindSymbol(library, "RootDoomIsRunning"));
    auto shutdown = reinterpret_cast<Shutdown>(gSystem->DynFindSymbol(library, "RootDoomShutdown"));
    auto renderer = reinterpret_cast<LaunchRenderer>(gSystem->DynFindSymbol(library, "RootDoomLaunchRenderer"));
    if (!launch || !running || !shutdown || !renderer || running() != 0) { fail(); return; }
    if (renderer("unused.wad", "unused-data", "invalid") != 1 || running() != 0) { fail(); return; }
    if (launch("unused.wad", "unused-data") != 1 || running() != 0) { fail(); return; }
    // Even a rejected launch installs the same compiled shutdown hook used by a native launch.
    gApplication->Emit("Terminate(Int_t)", 0);
    shutdown();
    if (running() != 0) { fail(); return; }
    std::cout << "ROOT_ENTRY_OK: shared symbols, batch rejection, ROOT shutdown signal\n";
    gSystem->Exit(0);
}
