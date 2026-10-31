// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/GeometryGL.h"
#include "OffscreenGL.h"
#include <GL/gl.h>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace rootdoom;
namespace {
void Require(bool v,const char* why) { if (!v) throw std::runtime_error(why); }
std::array<unsigned char,4> Pixel(int x,int y)
{ std::array<unsigned char,4> p{}; glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data()); return p; }
GeometryBatch Wall(std::shared_ptr<const GeometryTexture> texture,float x)
{
    GeometryBatch b{std::move(texture),{},true,1};
    GeometryVertex a{x,64,0,0,1,1}, c{x,-64,64,1,0,1};
    b.vertices={a,{x,-64,0,1,1,1},c,a,c,{x,64,64,0,0,1}}; return b;
}
}
int main()
{
    try {
        OffscreenGL context;
        if (!context.Open()) { std::cout<<"EGL software context unavailable\n"; return 77; }
        GeometryGL renderer;
        auto image=std::make_shared<GeometryTexture>(); image->id=1; image->width=image->height=2;
        image->rgba={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
        GeometryFrame frame; frame.world=true; frame.full_ui=false; frame.level=1; frame.eye_z=32;
        frame.world_batches.push_back(Wall(image,128));
        renderer.Draw(frame,640,400); glFinish();
        const auto tl=Pixel(240,272), tr=Pixel(400,272), bl=Pixel(240,192), br=Pixel(400,192);
        Require(tl[0]>250 && tl[1]<5 && tr[1]>250 && tr[0]<5 && bl[2]>250 && br[0]>250 && br[1]>250,"GL texture projection/orientation/RGB");
        auto cyan=std::make_shared<GeometryTexture>(); cyan->id=2; cyan->width=cyan->height=1; cyan->rgba={0,255,255,255};
        auto masked=std::make_shared<GeometryTexture>(*image); masked->id=3; masked->rgba[3]=0;
        frame.world_batches={Wall(cyan,256),Wall(masked,128)};
        renderer.Draw(frame,640,400); glFinish();
        const auto hole=Pixel(280,260);
        Require(hole[0]<5 && hole[1]>250 && hole[2]>250,"Alpha-tested geometry must reveal depth-tested surface behind it");
        const auto yellow=Pixel(400,192); Require(yellow[0]>250 && yellow[1]>250 && yellow[2]<5,"Near geometry depth ordering");
        renderer.Draw(frame,800,400); // pbuffer clips extra pixels; letterbox must remain black.
        const auto bar=Pixel(20,200); Require(bar[0]==0 && bar[1]==0 && bar[2]==0,"Geometry aspect-ratio bars");
        frame.full_ui=true; frame.ui.resize(640*400*4);
        for (unsigned i=0; i<640*400; ++i) { frame.ui[i*4]=12; frame.ui[i*4+1]=34; frame.ui[i*4+2]=56; frame.ui[i*4+3]=255; }
        renderer.Draw(frame,640,400); const auto ui=Pixel(320,200);
        Require(ui[0]==12 && ui[1]==34 && ui[2]==56,"Classic menu UI overlay");
        Require(glGetError()==GL_NO_ERROR,"OpenGL error in geometry renderer");
        std::cout<<"GEOMETRY_PIXELS_OK: projected textures, alpha, depth, aspect and UI; "<<glGetString(GL_RENDERER)<<'\n';
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
