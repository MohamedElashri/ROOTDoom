// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/RootGeometryPresenter.h"
#include "rootdoom/GeometryFrame.h"
#include "rootdoom/GeometryGL.h"
#include <TASImage.h>
#include <TCanvas.h>
#include <TVirtualGL.h>
#include <TGLSceneBase.h>
#include <TGLViewer.h>
#include <TGLWidget.h>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace rootdoom {
namespace {
class DoomScene final : public TGLSceneBase {
public:
    GeometryGL draw;
    std::shared_ptr<const GeometryFrame> frame;
    int width=1, height=1;
    bool wireframe=false;
    std::string error;
    GeometryCounts counts;
    std::uint64_t last_sequence=0;
    DoomScene()
    {
        SetNameTitle("ROOTDoom geometry", "Doom map triangles, textures and sprites");
        SetAutoDestruct(kFALSE); SetSelectable(kFALSE);
        fDoFrustumCheck=kFALSE; fDoClipCheck=kFALSE;
    }
    void CalcBoundingBox() const override
    {
        fBoundingBox=TGLBoundingBox(TGLVertex3(-65536,-65536,-65536),TGLVertex3(65536,65536,65536));
        fBoundingBoxValid=kTRUE;
    }
    void RenderOpaque(TGLRnrCtx&) override
    {
        try {
            if (!frame) return;
            draw.Draw(*frame,width,height,wireframe);
            if (frame->sequence!=last_sequence) {
                if (frame->world && !frame->full_ui) {
                    ++counts.world_frames; counts.triangles=frame->TriangleCount(); counts.sprites=frame->sprites;
                } else ++counts.ui_frames;
                counts.textures=draw.TextureCount(); last_sequence=frame->sequence;
            }
        }
        catch (const std::exception& e) { error=e.what(); }
    }
};
} // namespace
struct RootGeometryPresenter::Impl {
    TCanvas& canvas;
    GeometryMailbox& mailbox;
    DoomScene scene;
    TGLViewer* viewer=nullptr; // Owned by the canvas, detached before destruction.
    unsigned resizes=0, width=0, height=0;
    Impl(TCanvas& c, GeometryMailbox& m, bool wireframe) : canvas(c), mailbox(m)
    {
        if (canvas.GetGLDevice()<0) throw std::runtime_error("ROOT could not create an embedded OpenGL context; check GLX/display support");
        // The legacy canvas GL manager can create a context without running
        // TGLWidget's window-system loader. ROOT 6.40 then calls a null GLX
        // entry point from TGLContext::GetCurrent during the first viewer draw.
        // A temporary, unmapped ROOT widget uses ROOT's public initialization
        // path; restore the canvas context after destroying the temporary one.
        {
            std::unique_ptr<TGLWidget> bootstrap(TGLWidget::CreateDummy());
            if (!bootstrap || !bootstrap->MakeCurrent()) throw std::runtime_error("ROOT OpenGL loader initialization failed");
        }
        if (!gGLManager->MakeCurrent(canvas.GetGLDevice())) throw std::runtime_error("Cannot activate ROOT's canvas OpenGL context");
        // Embedded viewers render through the GL manager's readback pixmap.
        // A full embedded canvas does not allocate that device automatically.
        // It also supplies the viewer's viewport dimensions.
        Int_t viewport[4] = {};
        gGLManager->ExtractViewport(canvas.GetGLDevice(),viewport);
        if (viewport[2]<1 || viewport[3]<1) {
            if (!gGLManager->AttachOffScreenDevice(canvas.GetGLDevice(),0,0,canvas.GetWw(),canvas.GetWh())) {
                throw std::runtime_error("Cannot create ROOT's embedded OpenGL presentation device");
            }
        }
        // ROOT registers "gl" for the embedded TGLViewer; "ogl" creates a
        // standalone TGLSAViewer window even when the canvas has a GL device.
        viewer=dynamic_cast<TGLViewer*>(canvas.GetViewer3D("gl"));
        if (!viewer || viewer->GetGLWidget()) throw std::runtime_error("ROOT embedded OpenGL viewer is unavailable");
        scene.wireframe=wireframe;
        viewer->SetResetCamerasOnUpdate(kFALSE); viewer->SetDrawCameraCenter(kFALSE);
        viewer->AddScene(&scene);
        viewer->PostSceneBuildSetup(kTRUE);
    }
    ~Impl()
    {
        if (viewer) {
            gGLManager->MakeCurrent(canvas.GetGLDevice());
            scene.draw.Release(); viewer->RemoveScene(&scene);
            delete viewer; // TGLViewer releases the canvas's viewer pointer.
        }
    }
};
RootGeometryPresenter::RootGeometryPresenter(TCanvas& canvas, GeometryMailbox& mailbox, bool wireframe)
    : impl_(std::make_unique<Impl>(canvas,mailbox,wireframe)) {}
RootGeometryPresenter::~RootGeometryPresenter()=default;
PresentationTimes RootGeometryPresenter::Present(const std::vector<std::uint32_t>&)
{
    const auto begin=std::chrono::steady_clock::now();
    if (auto frame=impl_->mailbox.Consume()) impl_->scene.frame=std::move(frame);
    if (!impl_->scene.frame) return {};
    const unsigned w=impl_->canvas.GetWw(), h=impl_->canvas.GetWh();
    if (w!=impl_->width || h!=impl_->height) {
        if (impl_->width) ++impl_->resizes;
        impl_->width=w; impl_->height=h;
        if (w>1 && h>1) gGLManager->ResizeOffScreenDevice(impl_->canvas.GetGLDevice(),0,0,w,h);
    }
    impl_->scene.width=static_cast<int>(w); impl_->scene.height=static_cast<int>(h);
    // RequestDraw fills the readback pixmap; direct-copy tells ROOT to present
    // it in the native canvas as well. ROOT clears this flag after each draw.
    gGLManager->MarkForDirectCopy(impl_->canvas.GetGLDevice(),kTRUE);
    impl_->viewer->RequestDraw(TGLRnrCtx::kLODHigh);
    if (!impl_->scene.error.empty()) throw std::runtime_error(impl_->scene.error);
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    return {0,0,elapsed};
}
void RootGeometryPresenter::Capture(const std::string& path)
{
    if (std::filesystem::path(path).extension()!=".png") throw std::runtime_error("Geometry capture requires a PNG path");
    if (std::filesystem::path(path).has_parent_path()) std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    // X11's window-image capture does not reliably include a GL drawable.
    // Use the viewer's own back-buffer capture (which also redraws the scene)
    // instead of TASImage::FromPad's XGetImage path.
    std::unique_ptr<TImage> image(impl_->viewer->GetPictureUsingBB());
    if (!image || !image->GetWidth() || !image->GetHeight()) throw std::runtime_error("ROOT OpenGL capture is empty");
    image->WriteImage(path.c_str());
    if (!std::filesystem::exists(path) || !std::filesystem::file_size(path)) throw std::runtime_error("Cannot capture ROOT geometry canvas");
}
unsigned RootGeometryPresenter::ResizeCount() const { return impl_->resizes; }
GeometryCounts RootGeometryPresenter::GeometryStatistics() const { return impl_->scene.counts; }
} // namespace rootdoom
