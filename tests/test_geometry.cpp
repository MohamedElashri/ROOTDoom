// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/GeometryAssets.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
double Area(const rootdoom::MapPolygon& polygon)
{
    double area=0;
    for (std::size_t i=0; i<polygon.size(); ++i) {
        const auto& a=polygon[i]; const auto& b=polygon[(i+1)%polygon.size()]; area+=a.x*b.y-b.x*a.y;
    }
    return area/2;
}
}
int main()
{
    try {
        rootdoom::MapPolygon square{{-10,-10},{10,-10},{10,10},{-10,10}};
        const auto right=rootdoom::ClipMapPolygon(square,{0,0},{1,0},true);
        const auto left=rootdoom::ClipMapPolygon(square,{0,0},{1,0},false);
        Require(std::abs(Area(right)-200)<1e-6 && std::abs(Area(left)-200)<1e-6,"BSP half-plane area/winding");
        for (const auto& p:right) Require(p.y<=1e-6,"BSP right side sign");
        const auto empty=rootdoom::ClipMapPolygon(square,{0,-20},{1,0},true);
        Require(empty.empty(),"Outside polygon must disappear");
        rootdoom::DoomPalette palette{}; palette[5]={230,20,70}; palette[9]={10,220,30};
        // 2x2 patch: first column contains two pixels, second is transparent.
        std::vector<unsigned char> data{2,0,2,0,255,255,3,0,16,0,0,0,23,0,0,0,0,2,0,5,9,0,255,255};
        const auto texture=rootdoom::DecodeDoomPatch(data,palette);
        Require(texture.width==2 && texture.height==2 && texture.left==-1 && texture.top==3,"Patch metadata/endian");
        Require(texture.rgba[0]==230 && texture.rgba[1]==20 && texture.rgba[2]==70 && texture.rgba[3]==255,"Patch palette/channel order");
        Require(texture.rgba[7]==0 && texture.rgba[15]==0 && texture.rgba[8]==10 && texture.rgba[9]==220,"Patch transparency/row order");
        data.resize(20); bool rejected=false;
        try { rootdoom::DecodeDoomPatch(data,palette); } catch (const std::runtime_error&) { rejected=true; }
        Require(rejected,"Truncated patch accepted");
        rootdoom::GeometryMailbox mailbox;
        std::thread producer([&] {
            for (unsigned i=1; i<=2000; ++i) {
                auto frame=std::make_shared<rootdoom::GeometryFrame>(); frame->sequence=i;
                frame->ui.assign(1024,static_cast<unsigned char>(i)); mailbox.Publish(std::move(frame));
            }
        });
        producer.join();
        const auto last=mailbox.Consume();
        Require(last && last->sequence==2000 && !mailbox.Consume(),"Scene mailbox latest-frame/consume policy");
        for (auto c:last->ui) Require(c==static_cast<unsigned char>(2000),"Scene mailbox integrity");
        std::cout<<"GEOMETRY_CORE_OK: polygon clipping, patch pixels, immutable latest scene\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
