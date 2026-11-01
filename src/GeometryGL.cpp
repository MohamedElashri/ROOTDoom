// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/GeometryGL.h"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace rootdoom {
namespace {
constexpr double pi=3.14159265358979323846;
struct GLState {
    GLint matrix = GL_MODELVIEW;
    GLState()
    {
        glGetIntegerv(GL_MATRIX_MODE,&matrix);
        glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glMatrixMode(GL_MODELVIEW); glPushMatrix();
    }
    ~GLState()
    {
        glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glPopClientAttrib(); glPopAttrib(); glMatrixMode(matrix);
    }
};
void Ortho(double width, double height)
{
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
void Rectangle(float left, float top, float right, float bottom, float u0=0, float v0=0, float u1=1, float v1=1)
{
    glBegin(GL_QUADS);
    glTexCoord2f(u0,v0); glVertex2f(left,top); glTexCoord2f(u1,v0); glVertex2f(right,top);
    glTexCoord2f(u1,v1); glVertex2f(right,bottom); glTexCoord2f(u0,v1); glVertex2f(left,bottom);
    glEnd();
}
} // namespace
struct GeometryGL::Impl {
    std::map<std::uint64_t, GLuint> textures;
    GLuint ui=0;
    std::uint64_t level=0;
    GLuint Texture(const GeometryTexture& image)
    {
        const auto found=textures.find(image.id); if (found!=textures.end()) return found->second;
        if (image.width<1 || image.height<1 || image.rgba.size()!=std::size_t(image.width)*image.height*4) throw std::runtime_error("Invalid geometry texture");
        GLuint id=0; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,image.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,image.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        textures[image.id]=id; return id;
    }
    void Clear()
    {
        for (const auto& entry:textures) glDeleteTextures(1,&entry.second);
        textures.clear();
        if (ui) { glDeleteTextures(1,&ui); ui=0; }
    }
    void Batch(const GeometryBatch& batch, bool world)
    {
        if (!batch.texture) return;
        glBindTexture(GL_TEXTURE_2D,Texture(*batch.texture));
        if (world && !batch.fullbright) glEnable(GL_FOG); else glDisable(GL_FOG);
        if (batch.alpha<1) { glEnable(GL_BLEND); glDepthMask(GL_FALSE); }
        else { glDisable(GL_BLEND); glDepthMask(world ? GL_TRUE : GL_FALSE); }
        glBegin(GL_TRIANGLES);
        for (const auto& v:batch.vertices) { glColor4f(v.light,v.light,v.light,batch.alpha); glTexCoord2f(v.u,v.v); glVertex3f(v.x,v.y,v.z); }
        glEnd();
    }
    void UI(const GeometryFrame& frame)
    {
        if (frame.ui.empty()) return;
        if (frame.ui.size()!=640*400*4) throw std::runtime_error("Invalid geometry UI image");
        if (!ui) {
            glGenTextures(1,&ui); glBindTexture(GL_TEXTURE_2D,ui);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,640,400,0,GL_RGBA,GL_UNSIGNED_BYTE,frame.ui.data());
        } else { glBindTexture(GL_TEXTURE_2D,ui); glTexSubImage2D(GL_TEXTURE_2D,0,0,0,640,400,GL_RGBA,GL_UNSIGNED_BYTE,frame.ui.data()); }
        glColor4f(1,1,1,1); Rectangle(0,0,320,200);
    }
};
GeometryGL::GeometryGL() : impl_(std::make_unique<Impl>()) {}
GeometryGL::~GeometryGL() { Release(); }
void GeometryGL::Release() { impl_->Clear(); }
std::size_t GeometryGL::TextureCount() const { return impl_->textures.size(); }
void GeometryGL::Draw(const GeometryFrame& frame, int width, int height, bool wireframe)
{
    if (width<1 || height<1) return;
    if (frame.world && (frame.view_width<1 || frame.view_height<1 || frame.view_width>320 || frame.view_height>200)) throw std::runtime_error("Invalid geometry viewport");
    if (impl_->level!=frame.level) { impl_->Clear(); impl_->level=frame.level; }
    GLState state;
    glDisable(GL_SCISSOR_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    for (unsigned i=0; i<6; ++i) glDisable(GL_CLIP_PLANE0+i);
    glActiveTexture(GL_TEXTURE0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    glDepthMask(GL_TRUE); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    const int w=std::min(width,height*8/5), h=w*5/8, x=(width-w)/2, y=(height-h)/2;
    glViewport(x,y,w,h); glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.1f);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if (frame.world && !frame.full_ui) {
        const int vx=x+w*frame.view_x/320, vy=y+h*(200-frame.view_y-frame.view_height)/200;
        const int vw=w*frame.view_width/320, vh=h*frame.view_height/200;
        glViewport(vx,vy,vw,vh); Ortho(1,1); glColor4f(1,1,1,1);
        if (frame.sky) { glBindTexture(GL_TEXTURE_2D,impl_->Texture(*frame.sky)); Rectangle(0,0,1,1,float(frame.angle*2/pi+0.5),0,float(frame.angle*2/pi-0.5),1); }
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        const double ratio=double(frame.view_height)/frame.view_width;
        glFrustum(-1,1,-ratio,ratio,1,65536);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        const double s=std::sin(frame.angle), c=std::cos(frame.angle);
        const GLdouble camera[16]={s,0,-c,0, -c,0,-s,0, 0,1,0,0,
            -s*frame.eye_x+c*frame.eye_y,-frame.eye_z,c*frame.eye_x+s*frame.eye_y,1};
        glMultMatrixd(camera);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
        glFogi(GL_FOG_MODE,GL_LINEAR); glFogf(GL_FOG_START,0); glFogf(GL_FOG_END,3072);
        const GLfloat black[4]={0,0,0,1}; glFogfv(GL_FOG_COLOR,black);
        if (wireframe) { glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); glDisable(GL_TEXTURE_2D); }
        for (const auto& batch:frame.world_batches) if (batch.alpha>=1) impl_->Batch(batch,true);
        std::vector<const GeometryBatch*> translucent;
        for (const auto& batch:frame.world_batches) if (batch.alpha<1 && !batch.vertices.empty()) translucent.push_back(&batch);
        auto distance=[&](const GeometryBatch* batch) { const auto& v=batch->vertices[0]; return std::hypot(v.x-frame.eye_x,v.y-frame.eye_y); };
        std::sort(translucent.begin(),translucent.end(),[&](const auto* a,const auto* b) { return distance(a)>distance(b); });
        for (const auto* batch:translucent) impl_->Batch(*batch,true);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); glEnable(GL_TEXTURE_2D);
        glDisable(GL_FOG); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDepthMask(GL_FALSE);
        Ortho(frame.view_width,frame.view_height);
        for (const auto& batch:frame.weapon_batches) impl_->Batch(batch,false);
        if (frame.tint[3]>0) {
            glDisable(GL_TEXTURE_2D); glEnable(GL_BLEND); glDisable(GL_ALPHA_TEST);
            glColor4fv(frame.tint.data()); Rectangle(0,0,float(frame.view_width),float(frame.view_height));
            glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST);
        }
    }
    glViewport(x,y,w,h); Ortho(320,200); glDisable(GL_FOG); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); impl_->UI(frame);
}
} // namespace rootdoom
