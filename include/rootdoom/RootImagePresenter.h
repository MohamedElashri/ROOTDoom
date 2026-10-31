// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class TCanvas;
class TPad;

namespace rootdoom {

class MutableRootImage;

struct PresentationTimes {
    double copy_ms = 0;
    double image_ms = 0;
    double paint_ms = 0;
};

// Main-thread-only. The canvas outlives the presenter; no pad owns this image.
class RootImagePresenter {
public:
    explicit RootImagePresenter(TCanvas& canvas);
    ~RootImagePresenter();
    RootImagePresenter(const RootImagePresenter&) = delete;
    RootImagePresenter& operator=(const RootImagePresenter&) = delete;

    PresentationTimes Present(const std::vector<std::uint32_t>& rgb);
    void Capture(const std::string& path);
    unsigned ResizeCount() const { return resize_count_; }
    int SceneObjectCount() const;

private:
    void FitPad();
    TCanvas& canvas_;
    std::unique_ptr<TPad> pad_;
    std::unique_ptr<MutableRootImage> image_;
    unsigned last_width_ = 0;
    unsigned last_height_ = 0;
    unsigned resize_count_ = 0;
};

} // namespace rootdoom
