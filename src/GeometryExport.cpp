// SPDX-License-Identifier: GPL-2.0-or-later
#include "GeometryExport.h"
#include "rootdoom/GeometryAssets.h"
extern "C" {
#include "doomstat.h"
#include "r_state.h"
#include "r_main.h"
#include "r_sky.h"
#include "p_local.h"
}
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace rootdoom {
namespace {
constexpr double pi = 3.14159265358979323846;
float Units(fixed_t n) { return static_cast<float>(n / 65536.0); }
MapPoint Point(vertex_t* v) { return {Units(v->x), Units(v->y)}; }
float Light(int value) { return std::clamp(value / 255.f, 0.08f, 1.f); }
void Quad(GeometryBatch& batch, GeometryVertex a, GeometryVertex b, GeometryVertex c, GeometryVertex d)
{ batch.vertices.insert(batch.vertices.end(), {a,b,c,a,c,d}); }
} // namespace
struct GeometryExporter::Impl {
    struct Leaf { int sector; MapPolygon polygon; };
    GeometryAssets assets;
    GeometryMailbox& mailbox;
    std::vector<Leaf> leaves;
    std::uint64_t level = 0, built_level = 0, sequence = 0;
    std::map<std::uint64_t, std::size_t> batches;
    explicit Impl(const std::string& iwad, GeometryMailbox& box) : assets(iwad), mailbox(box) {}
    void Walk(unsigned child, MapPolygon polygon, unsigned depth)
    {
        if (polygon.size() < 3) return;
        if (depth > 256) throw std::runtime_error("Doom BSP exceeds geometry depth limit");
        if (child & NF_SUBSECTOR) {
            const unsigned index = child & ~NF_SUBSECTOR;
            if (index >= static_cast<unsigned>(numsubsectors)) throw std::runtime_error("Invalid Doom BSP leaf");
            const auto& sub = subsectors[index];
            for (int i = 0; i < sub.numlines && polygon.size() >= 3; ++i) {
                const auto& seg = segs[sub.firstline+i];
                const auto a = Point(seg.v1), b = Point(seg.v2);
                polygon = ClipMapPolygon(polygon, a, {b.x-a.x,b.y-a.y}, true);
            }
            if (polygon.size() >= 3) leaves.push_back({static_cast<int>(sub.sector-sectors), std::move(polygon)});
        } else {
            if (child >= static_cast<unsigned>(numnodes)) throw std::runtime_error("Invalid Doom BSP node");
            const auto& node = nodes[child];
            MapPoint origin{Units(node.x),Units(node.y)}, direction{Units(node.dx),Units(node.dy)};
            for (unsigned side = 0; side < 2; ++side) Walk(node.children[side], ClipMapPolygon(polygon,origin,direction,side==0),depth+1);
        }
    }
    void Topology()
    {
        leaves.clear();
        if (numvertexes < 3) throw std::runtime_error("Empty Doom level geometry");
        double minx = 1e10, miny = 1e10, maxx = -1e10, maxy = -1e10;
        for (int i = 0; i < numvertexes; ++i) {
            const auto p = Point(&vertexes[i]);
            minx=std::min(minx,p.x); miny=std::min(miny,p.y); maxx=std::max(maxx,p.x); maxy=std::max(maxy,p.y);
        }
        Walk(numnodes ? static_cast<unsigned>(numnodes-1) : NF_SUBSECTOR,
             {{minx-1,miny-1},{maxx+1,miny-1},{maxx+1,maxy+1},{minx-1,maxy+1}},0);
        built_level=level;
    }
    GeometryBatch& Batch(GeometryFrame& frame, std::shared_ptr<const GeometryTexture> texture)
    {
        const auto found = batches.find(texture->id);
        if (found != batches.end()) return frame.world_batches[found->second];
        batches[texture->id] = frame.world_batches.size();
        frame.world_batches.push_back({std::move(texture),{},false,1});
        return frame.world_batches.back();
    }
    void Planes(GeometryFrame& frame)
    {
        const int sky = assets.Find("F_SKY1");
        for (const auto& leaf : leaves) {
            const auto& sector = sectors[leaf.sector];
            if (sector.ceilingheight <= sector.floorheight) continue;
            for (bool ceiling : {false,true}) {
                const int flat = ceiling ? sector.ceilingpic : sector.floorpic;
                if (firstflat+flat == sky) continue;
                auto texture = assets.Flat(firstflat+flattranslation[flat]);
                auto& batch = Batch(frame,texture);
                const float z=Units(ceiling ? sector.ceilingheight : sector.floorheight), light=Light(sector.lightlevel);
                auto vertex = [&](MapPoint p) { return GeometryVertex{float(p.x),float(p.y),z,float(p.x/64),float(-p.y/64),light}; };
                for (std::size_t i=1; i+1<leaf.polygon.size(); ++i) {
                    const auto a=vertex(leaf.polygon[0]), b=vertex(leaf.polygon[i]), c=vertex(leaf.polygon[i+1]);
                    batch.vertices.insert(batch.vertices.end(), ceiling ? std::initializer_list<GeometryVertex>{a,c,b} : std::initializer_list<GeometryVertex>{a,b,c});
                }
            }
        }
    }
    void Wall(GeometryFrame& frame, MapPoint a, MapPoint b, float low, float high,
              int index, float offset, float top, float light)
    {
        if (index <= 0 || high <= low) return;
        const auto texture = assets.Wall(texturetranslation[index]);
        const float u0=offset/texture->width, u1=u0+float(std::hypot(b.x-a.x,b.y-a.y))/texture->width;
        const float v0=(top-low)/texture->height, v1=(top-high)/texture->height;
        auto& batch = Batch(frame,texture);
        Quad(batch,{float(a.x),float(a.y),low,u0,v0,light},{float(b.x),float(b.y),low,u1,v0,light},
                   {float(b.x),float(b.y),high,u1,v1,light},{float(a.x),float(a.y),high,u0,v1,light});
    }
    void Walls(GeometryFrame& frame)
    {
        const int sky = assets.Find("F_SKY1");
        for (int i=0; i<numlines; ++i) for (int side=0; side<2; ++side) {
            const auto& line=lines[i];
            if (line.sidenum[side] < 0) continue;
            const auto& sd=sides[line.sidenum[side]];
            const auto& front=*sd.sector;
            const sector_t* back = side ? line.frontsector : line.backsector;
            const MapPoint a=Point(side ? line.v2 : line.v1), b=Point(side ? line.v1 : line.v2);
            const float floor=Units(front.floorheight), ceiling=Units(front.ceilingheight);
            const float row=Units(sd.rowoffset), offset=Units(sd.textureoffset);
            const int contrast = line.dy == 0 ? -16 : line.dx == 0 ? 16 : 0;
            const float light=Light(front.lightlevel+contrast);
            auto height = [&](int texture) { return texture > 0 ? float(assets.Wall(texturetranslation[texture])->height) : 0.f; };
            if (!back) {
                const float top = line.flags & ML_DONTPEGBOTTOM ? floor+height(sd.midtexture) : ceiling;
                Wall(frame,a,b,floor,ceiling,sd.midtexture,offset,top+row,light);
                continue;
            }
            const float bf=Units(back->floorheight), bc=Units(back->ceilingheight);
            if (ceiling>bc && !(firstflat+front.ceilingpic == sky && firstflat+back->ceilingpic == sky)) {
                const float top = line.flags & ML_DONTPEGTOP ? ceiling : bc+height(sd.toptexture);
                Wall(frame,a,b,std::max(bc,floor),ceiling,sd.toptexture,offset,top+row,light);
            }
            if (bf>floor) {
                const float top = line.flags & ML_DONTPEGBOTTOM ? ceiling : bf;
                Wall(frame,a,b,floor,std::min(bf,ceiling),sd.bottomtexture,offset,top+row,light);
            }
            if (sd.midtexture > 0) {
                const float openlow=std::max(floor,bf), openhigh=std::min(ceiling,bc), h=height(sd.midtexture);
                const float top=(line.flags & ML_DONTPEGBOTTOM ? openlow+h : openhigh)+row;
                Wall(frame,a,b,std::max(openlow,top-h),std::min(openhigh,top),sd.midtexture,offset,top,light);
            }
        }
    }
    void Sprites(GeometryFrame& frame)
    {
        const float rx=std::sin(frame.angle), ry=-std::cos(frame.angle);
        for (int s=0; s<numsectors; ++s) for (const auto* thing=sectors[s].thinglist; thing; thing=thing->snext) {
            if (thing==players[displayplayer].mo || int(thing->sprite)>=numsprites) continue;
            const auto& def=sprites[thing->sprite];
            const int sprite_frame=thing->frame & FF_FRAMEMASK;
            if (sprite_frame<0 || sprite_frame>=def.numframes) continue;
            const auto& sf=def.spriteframes[sprite_frame];
            unsigned rotation=0;
            if (sf.rotate) {
                const double a=std::atan2(Units(thing->y)-frame.eye_y,Units(thing->x)-frame.eye_x);
                const double orientation=thing->angle * (2*pi/4294967296.0);
                double turn=std::fmod(a-orientation+9*pi/8,2*pi); if (turn<0) turn+=2*pi;
                rotation=static_cast<unsigned>(turn/(pi/4)) & 7;
            }
            const auto texture=assets.Patch(firstspritelump+sf.lump[rotation]);
            const float x=Units(thing->x), y=Units(thing->y), top=Units(thing->z)+texture->top;
            const float left=-float(texture->left), right=left+texture->width, bottom=top-texture->height;
            const float u0=sf.flip[rotation] ? 1.f : 0.f, u1=1-u0;
            const bool bright=(thing->frame & FF_FULLBRIGHT)!=0;
            const float light=bright ? 1.f : Light(sectors[s].lightlevel);
            GeometryBatch batch{texture,{},bright,(thing->flags & MF_SHADOW) ? 0.35f : 1.f};
            Quad(batch,{x+rx*left,y+ry*left,bottom,u0,1,light},{x+rx*right,y+ry*right,bottom,u1,1,light},
                 {x+rx*right,y+ry*right,top,u1,0,light},{x+rx*left,y+ry*left,top,u0,0,light});
            frame.world_batches.push_back(std::move(batch)); ++frame.sprites;
        }
    }
    void Weapons(GeometryFrame& frame)
    {
        const auto& player=players[displayplayer];
        for (const auto& psp : player.psprites) {
            if (!psp.state) continue;
            const auto& def=sprites[psp.state->sprite];
            const auto& sf=def.spriteframes[psp.state->frame & FF_FRAMEMASK];
            const auto texture=assets.Patch(firstspritelump+sf.lump[0]);
            const float scale=frame.view_width/320.f;
            const float x=(Units(psp.sx)-texture->left)*scale;
            const float y=frame.view_height/2.f+(Units(psp.sy)-texture->top-100.5f)*scale;
            const float u0=sf.flip[0] ? 1.f : 0.f, u1=1-u0;
            const float light=(psp.state->frame & FF_FULLBRIGHT) ? 1.f : Light(player.mo->subsector->sector->lightlevel);
            GeometryBatch batch{texture,{},true,player.powers[pw_invisibility] ? 0.35f : 1.f};
            Quad(batch,{x,y,0,u0,0,light},{x+texture->width*scale,y,0,u1,0,light},
                 {x+texture->width*scale,y+texture->height*scale,0,u1,1,light},{x,y+texture->height*scale,0,u0,1,light});
            frame.weapon_batches.push_back(std::move(batch));
        }
    }
    void Publish(const std::uint32_t* rgb)
    {
        auto frame=std::make_shared<GeometryFrame>();
        frame->sequence=++sequence; frame->level=level; frame->map=gamemap; frame->tic=leveltime;
        frame->world=gamestate==GS_LEVEL && level && players[displayplayer].mo;
        frame->full_ui=!frame->world || menuactive || automapactive || paused;
        if (frame->world) {
            if (built_level!=level) Topology();
            frame->eye_x=Units(viewx); frame->eye_y=Units(viewy); frame->eye_z=Units(viewz);
            frame->angle=float(viewangle*(2*pi/4294967296.0));
            frame->view_x=viewwindowx; frame->view_y=viewwindowy;
            frame->view_width=scaledviewwidth; frame->view_height=viewheight;
            batches.clear(); Planes(*frame); Walls(*frame); Sprites(*frame); Weapons(*frame);
            frame->sky=assets.Wall(skytexture);
            const auto& player=players[displayplayer];
            if (player.damagecount) frame->tint={1,0,0,std::min(0.55f,player.damagecount/100.f)};
            else if (player.bonuscount) frame->tint={1,0.8f,0,std::min(0.25f,player.bonuscount/100.f)};
        }
        frame->ui.resize(640*400*4);
        for (int y=0; y<400; ++y) for (int x=0; x<640; ++x) {
            const auto color=rgb[y*640+x]; auto* out=&frame->ui[(y*640+x)*4];
            out[0]=static_cast<unsigned char>(color>>16); out[1]=static_cast<unsigned char>(color>>8); out[2]=static_cast<unsigned char>(color);
            const bool view=x>=frame->view_x*2 && x<(frame->view_x+frame->view_width)*2 &&
                            y>=frame->view_y*2 && y<(frame->view_y+frame->view_height)*2;
            out[3]=frame->full_ui || !view ? 255 : 0;
        }
        mailbox.Publish(std::move(frame));
    }
};
GeometryExporter::GeometryExporter(const std::string& iwad, GeometryMailbox& mailbox) : impl_(std::make_unique<Impl>(iwad,mailbox)) {}
GeometryExporter::~GeometryExporter()=default;
void GeometryExporter::NewLevel() { ++impl_->level; }
void GeometryExporter::Publish(const std::uint32_t* framebuffer) { impl_->Publish(framebuffer); }
} // namespace rootdoom
