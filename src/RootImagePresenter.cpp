// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/RootImagePresenter.h"
#include "rootdoom/Frame.h"

#include <TASImage.h>
#include <TCanvas.h>
#include <TList.h>
#include <TPad.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace rootdoom {
namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::duration delta) { return std::chrono::duration<double, std::milli>(delta).count(); }
} // namespace

// ROOT's GetArgbArray() prefers fScaledImage after a scaled paint. Clear it
// before acquiring the original buffer, or updates can hit stale/wrong-size data.
// This protected member is verified against ROOT 6.40.04's TASImage implementation.
class MutableRootImage final : public TASImage {
public:
    MutableRootImage() : TASImage(kFrameWidth, kFrameHeight) {}
    void InvalidateScaledImage()
    {
        delete fScaledImage;
        fScaledImage = nullptr;
    }
};

RootImagePresenter::RootImagePresenter(TCanvas& canvas) : canvas_(canvas),
    pad_(std::make_unique<TPad>("rootdoom_pixels", "ROOTDoom framebuffer", 0, 0, 1, 1)),
    image_(std::make_unique<MutableRootImage>())
{
    canvas_.SetFillColor(1);
    canvas_.SetBorderMode(0);
    canvas_.cd();
    pad_->SetFillColor(1);
    pad_->SetBorderMode(0);
    pad_->SetMargin(0, 0, 0, 0);
    pad_->ResetBit(TObject::kCanDelete);
    pad_->Draw();
    pad_->cd();
    image_->SetImageQuality(TAttImage::kImgFast);
    image_->ResetBit(TObject::kCanDelete);
    image_->SetBit(TObject::kCannotPick);
    image_->Draw("X");
    canvas_.cd();
}

RootImagePresenter::~RootImagePresenter()
{
    pad_->GetListOfPrimitives()->Remove(image_.get());
    canvas_.GetListOfPrimitives()->Remove(pad_.get());
}

void RootImagePresenter::FitPad()
{
    const unsigned width = canvas_.GetWw();
    const unsigned height = canvas_.GetWh();
    if (width == last_width_ && height == last_height_) {
        return;
    }
    if (last_width_ != 0) {
        ++resize_count_;
    }
    last_width_ = width;
    last_height_ = height;
    if (width == 0 || height == 0) {
        return;
    }
    const double aspect = static_cast<double>(kFrameWidth) / kFrameHeight;
    const double canvas_aspect = static_cast<double>(width) / height;
    if (canvas_aspect > aspect) {
        const double fraction = aspect / canvas_aspect;
        pad_->SetPad((1 - fraction) / 2, 0, (1 + fraction) / 2, 1);
    } else {
        const double fraction = canvas_aspect / aspect;
        pad_->SetPad(0, (1 - fraction) / 2, 1, (1 + fraction) / 2);
    }
    canvas_.Modified();
}

PresentationTimes RootImagePresenter::Present(const std::vector<std::uint32_t>& rgb)
{
    if (rgb.size() != kFramePixels) {
        throw std::invalid_argument("Framebuffer must contain exactly 640 x 400 RGB pixels");
    }
    FitPad();
    const auto begin = Clock::now();
    image_->InvalidateScaledImage();
    image_->BeginPaint();
    auto* destination = image_->GetArgbArray();
    if (!destination) {
        throw std::runtime_error("ROOT did not provide an ARGB framebuffer");
    }
    const auto copy_begin = Clock::now();
    std::transform(rgb.begin(), rgb.end(), destination, OpaqueArgb);
    const auto copy_end = Clock::now();
    image_->EndPaint();
    const auto paint_begin = Clock::now();
    pad_->Modified();
    canvas_.Modified();
    canvas_.Update();
    const auto end = Clock::now();
    return {Milliseconds(copy_end - copy_begin),
            Milliseconds(paint_begin - begin) - Milliseconds(copy_end - copy_begin),
            Milliseconds(end - paint_begin)};
}

void RootImagePresenter::Capture(const std::string& path)
{
    const std::filesystem::path file(path);
    if (file.extension() != ".png") {
        throw std::invalid_argument("Screenshot path must end in .png");
    }
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path());
    }
    TASImage snapshot;
    // Native mode captures the real ROOT drawable. Batch mode uses TImageDump.
    // FromPad pumps events/sleeps on X11: call only after stopping the demo timer.
    snapshot.FromPad(&canvas_);
    if (snapshot.GetWidth() == 0 || snapshot.GetHeight() == 0) {
        throw std::runtime_error("ROOT canvas capture returned an empty image");
    }
    snapshot.WriteImage(path.c_str());
    if (!std::filesystem::exists(file) || std::filesystem::file_size(file) == 0) {
        throw std::runtime_error("ROOT could not write screenshot: " + path);
    }
}

int RootImagePresenter::SceneObjectCount() const
{
    return canvas_.GetListOfPrimitives()->GetSize() + pad_->GetListOfPrimitives()->GetSize();
}

} // namespace rootdoom
