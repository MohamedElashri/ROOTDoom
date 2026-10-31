// SPDX-License-Identifier: GPL-2.0-or-later
// Run in an existing native ROOT session: .x macros/doom.C("/path/to/doom1.wad")
#include <TSystem.h>
#include <TString.h>
#include <iostream>

void doom(const char* iwad, const char* data_directory = "rootdoom-data")
{
    TString here = gSystem->DirName(__FILE__);
    if (!here.BeginsWith("/")) here = TString(gSystem->WorkingDirectory()) + "/" + here;
    TString library;
    if (const char* configured = gSystem->Getenv("ROOTDOOM_LIBRARY")) library = configured;
    else {
        const TString base = here + "/../";
        for (const char* build : {"build", "build-local"}) {
            TString candidate = base + build + "/librootdoom." + gSystem->GetSoExt();
            if (!gSystem->AccessPathName(candidate)) { library = candidate; break; }
        }
    }
    if (library.IsNull() || gSystem->Load(library) < 0) {
        std::cerr << "Build librootdoom first, or set ROOTDOOM_LIBRARY to its absolute path.\n";
        return;
    }
    using Launch = int (*)(const char*, const char*);
    auto launch = reinterpret_cast<Launch>(gSystem->DynFindSymbol(library, "RootDoomLaunch"));
    if (!launch) { std::cerr << "RootDoomLaunch symbol not found in " << library << '\n'; return; }
    launch(iwad, data_directory);
}
