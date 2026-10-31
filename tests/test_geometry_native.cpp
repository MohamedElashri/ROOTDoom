// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/RootDoomWindow.h"
#include "rootdoom/RootGeometryPresenter.h"
#include "rootdoom/GeometryFrame.h"
#include "rootdoom/InputQueue.h"
#include <TApplication.h>
#include <TCanvas.h>
#include <TASImage.h>
#include <TROOT.h>
#include <TSystem.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    try {
        TApplication app("ROOTDoomGeometryTest",&argc,argv);
        if (gROOT->IsBatch()) throw std::runtime_error("Native ROOT/OpenGL display unavailable");
        rootdoom::InputQueue input;
        rootdoom::RootDoomWindow window(&input,{},true);
        rootdoom::GeometryMailbox mailbox;
        rootdoom::RootGeometryPresenter presenter(window.Canvas(),mailbox,false);
        auto texture=std::make_shared<rootdoom::GeometryTexture>();
        texture->id=1; texture->width=texture->height=2;
        texture->rgba={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
        auto frame=std::make_shared<rootdoom::GeometryFrame>();
        frame->sequence=1; frame->level=1; frame->world=true; frame->full_ui=false; frame->eye_z=32;
        rootdoom::GeometryBatch wall{texture,{},true,1};
        wall.vertices={{128,64,0,0,1,1},{128,-64,0,1,1,1},{128,-64,64,1,0,1},
                       {128,64,0,0,1,1},{128,-64,64,1,0,1},{128,64,64,0,0,1}};
        frame->world_batches.push_back(std::move(wall)); mailbox.Publish(frame);
        const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now()<end) { gSystem->ProcessEvents(); presenter.Present({}); gSystem->Sleep(5); }
        if (presenter.GeometryStatistics().world_frames!=1) throw std::runtime_error("ROOT viewer did not execute the geometry scene");
        presenter.Capture("native-geometry-colors.png");
        TASImage image("native-geometry-colors.png"); auto* pixels=image.GetArgbArray();
        if (!pixels) throw std::runtime_error("Native geometry capture is empty");
        const auto w=image.GetWidth(),h=image.GetHeight();
        const auto red=pixels[(h*32/100)*w+w*3/8] & 0xffffffu;
        const auto green=pixels[(h*32/100)*w+w*5/8] & 0xffffffu;
        const auto blue=pixels[(h*52/100)*w+w*3/8] & 0xffffffu;
        if (red!=0xff0000 || green!=0x00ff00 || blue!=0x0000ff) {
            std::cerr<<"Captured RGB samples: "<<std::hex<<red<<", "<<green<<", "<<blue<<std::dec<<'\n';
            throw std::runtime_error("ROOT embedded geometry pixels have wrong colors/orientation");
        }
        std::cout<<"ROOT_GEOMETRY_NATIVE_OK: embedded TGLViewer, real textured triangles and captured RGB pixels\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
