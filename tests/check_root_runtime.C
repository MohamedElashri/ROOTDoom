// SPDX-License-Identifier: GPL-2.0-or-later
// Batch dependency probe: validates ROOT dictionaries and graphics without X11.
#include <TASImage.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TList.h>
#include <TSystem.h>

#include <iostream>

void check_root_runtime()
{
    if (gSystem->Load("libGui") < 0 || gSystem->Load("libASImage") < 0) {
        std::cerr << "ROOT_RUNTIME_FAILED: native libraries unavailable\n";
        gSystem->Exit(1);
        return;
    }
    {
        TFile file("root-runtime.root", "RECREATE");
        if (file.IsZombie()) {
            gSystem->Exit(1);
            return;
        }
        TH1D histogram("runtime", "Dictionary roundtrip", 10, 0, 1);
        histogram.Fill(0.5, 3.0);
        histogram.Write();
    }
    {
        TFile file("root-runtime.root", "READ");
        TH1D* histogram = nullptr;
        file.GetObject("runtime", histogram);
        if (!histogram || histogram->GetEntries() != 1 || histogram->GetBinContent(6) != 3) {
            std::cerr << "ROOT_RUNTIME_FAILED: histogram dictionary roundtrip failed\n";
            gSystem->Exit(1);
            return;
        }
    }
    {
        TCanvas canvas("runtime_canvas", "Batch graphics probe", 128, 80);
        TASImage image(16, 16);
        image.BeginPaint();
        auto* pixels = image.GetArgbArray();
        if (!pixels) {
            std::cerr << "ROOT_RUNTIME_FAILED: image pixels unavailable\n";
            gSystem->Exit(1);
            return;
        }
        for (int i = 0; i < 16 * 16; ++i) {
            pixels[i] = 0xffff0000U;
        }
        image.EndPaint();
        image.Draw("X");
        canvas.Modified();
        canvas.Update();
        canvas.Print("root-runtime.png");
        canvas.GetListOfPrimitives()->Remove(&image);
    }
    TASImage saved("root-runtime.png");
    if (saved.GetWidth() == 0 || saved.GetHeight() == 0) {
        std::cerr << "ROOT_RUNTIME_FAILED: batch canvas export failed\n";
        gSystem->Exit(1);
        return;
    }
    std::cout << "ROOT_RUNTIME_OK\n";
    gSystem->Exit(0);
}
