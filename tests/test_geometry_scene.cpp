// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/DoomEngine.h"
#include "rootdoom/DoomRunner.h"
#include "rootdoom/GeometryFrame.h"
#include "doomkeys.h"
#ifdef ROOTDOOM_TEST_EGL
#include "rootdoom/GeometryGL.h"
#include "OffscreenGL.h"
#include <GL/gl.h>
#include <TASImage.h>
#include <TROOT.h>
#endif
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
namespace {
void Require(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
std::shared_ptr<const rootdoom::GeometryFrame> Wait(rootdoom::GeometryMailbox& box, rootdoom::DoomRunner& runner,
    const std::function<bool(const rootdoom::GeometryFrame&)>& predicate, const char* stage)
{
    const auto deadline=std::chrono::steady_clock::now()+8s;
    while (std::chrono::steady_clock::now()<deadline && runner.State()==rootdoom::WorkerState::Running) {
        if (auto frame=box.Consume(); frame && predicate(*frame)) return frame;
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error(std::string("Scene did not reach ")+stage+": "+runner.Error());
}
void Tap(rootdoom::InputQueue& input,unsigned char key)
{ input.SetKey(key,true); std::this_thread::sleep_for(80ms); input.SetKey(key,false); std::this_thread::sleep_for(80ms); }
#ifdef ROOTDOOM_TEST_EGL
void Capture(const rootdoom::GeometryFrame& frame)
{
    rootdoom::GeometryGL renderer; renderer.Draw(frame,640,400); glFinish();
    Require(glGetError()==GL_NO_ERROR,"Real-level OpenGL draw failed");
    std::vector<unsigned char> pixels(640*400*4); glReadPixels(0,0,640,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    unsigned visible=0; for (unsigned i=0; i<640*336; ++i) if (pixels[(i+640*64)*4] || pixels[(i+640*64)*4+1] || pixels[(i+640*64)*4+2]) ++visible;
    Require(visible>640*168,"Real geometry view is mostly blank");
    gROOT->SetBatch(kTRUE); TASImage image(640,400); image.BeginPaint(); auto* out=image.GetArgbArray();
    for (unsigned y=0; y<400; ++y) for (unsigned x=0; x<640; ++x) {
        const auto* p=&pixels[((399-y)*640+x)*4]; out[y*640+x]=0xff000000u | (unsigned(p[0])<<16) | (unsigned(p[1])<<8) | p[2];
    }
    image.EndPaint(); image.WriteImage("geometry-level.png");
    renderer.Draw(frame,640,400,true); glFinish(); glReadPixels(0,0,640,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    image.BeginPaint(); out=image.GetArgbArray();
    for (unsigned y=0; y<400; ++y) for (unsigned x=0; x<640; ++x) { const auto* p=&pixels[((399-y)*640+x)*4]; out[y*640+x]=0xff000000u|(unsigned(p[0])<<16)|(unsigned(p[1])<<8)|p[2]; }
    image.EndPaint(); image.WriteImage("geometry-wireframe.png");
}
#endif
}
int main(int argc,char** argv)
{
    try {
        Require(argc>=2,"Expected IWAD path");
#ifdef ROOTDOOM_TEST_EGL
        OffscreenGL gl;
        if (argc>2 && !gl.Open()) { std::cout<<"EGL context unavailable\n"; return 77; }
#endif
        rootdoom::GeometryMailbox scenes; rootdoom::FrameMailbox frames; rootdoom::InputQueue input; rootdoom::EngineReport report;
        rootdoom::EngineOptions options{argv[1],"geometry-test-data",{"-warp","1","1"},false,&scenes};
        rootdoom::DoomRunner runner;
        runner.Start([&](rootdoom::Clock& clock) { rootdoom::RunDoom(clock,frames,input,options,report,[](const std::string&){}); });
        const auto initial=Wait(scenes,runner,[&](const auto& f) { return report.initialized.load() && f.world && !f.full_ui && f.TriangleCount()>100; },"initialized E1M1");
        Require(initial->map==1 && initial->level==1 && initial->sprites>0 && !initial->weapon_batches.empty(),"E1M1 entities/weapon missing");
        Require(initial->ui[3]==0 && initial->ui.back()==255,"3D world covered by software framebuffer");
        for (const auto& batch:initial->world_batches) {
            Require(batch.texture && !batch.texture->rgba.empty() && batch.vertices.size()%3==0,"Invalid textured triangle batch");
            for (const auto& v:batch.vertices) Require(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(v.x)<100000 && std::abs(v.y)<100000,"Invalid map geometry coordinate");
        }
        input.SetKey(KEY_UPARROW,true); std::this_thread::sleep_for(350ms); input.SetKey(KEY_UPARROW,false);
        const auto moved=Wait(scenes,runner,[&](const auto& f) { return f.eye_x!=initial->eye_x || f.eye_y!=initial->eye_y; },"moving camera");
        Require(moved->sequence>initial->sequence,"Camera scene did not advance");
        Tap(input,KEY_ESCAPE); Wait(scenes,runner,[](const auto& f) { return f.full_ui; },"menu");
        Tap(input,KEY_ESCAPE);
        const auto displayed=Wait(scenes,runner,[](const auto& f) { return f.world && !f.full_ui; },"menu closed");
        for (unsigned char key:std::string("idclev12")) Tap(input,key);
        const auto next=Wait(scenes,runner,[](const auto& f) { return f.world && f.map==2 && f.level==2 && !f.full_ui; },"E1M2 transition");
        runner.RequestStop(); runner.Join();
        Require(runner.State()==rootdoom::WorkerState::Finished,"Geometry worker did not stop cleanly");
        Require(report.allocations_after_cleanup==0 && report.files_after_cleanup==0,"Engine cleanup failed");
        Require(report.software_views_skipped>0,"Doom's software world renderer was not bypassed");
        Require(initial->world_batches.front().texture->rgba.size()>0 && next->TriangleCount()>100,"Scene assets did not survive worker cleanup");
#ifdef ROOTDOOM_TEST_EGL
        if (argc>2) Capture(*displayed);
#endif
        std::cout<<"GEOMETRY_SCENE_OK: E1M1 "<<initial->TriangleCount()<<" triangles, "<<initial->sprites<<" sprites; camera/menu/level transition; immutable assets survive cleanup\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
