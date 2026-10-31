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
struct GeometryCounts {
    std::uint64_t world_frames = 0, ui_frames = 0;
    std::size_t triangles = 0, sprites = 0, textures = 0;
};
class FramePresenter {
public:
    virtual ~FramePresenter() = default;
    virtual PresentationTimes Present(const std::vector<std::uint32_t>& rgb) = 0;
    virtual void Capture(const std::string& path) = 0;
    virtual unsigned ResizeCount() const = 0;
    virtual int SceneObjectCount() const = 0;
    virtual GeometryCounts GeometryStatistics() const { return {}; }
};

// Main-thread-only. The canvas outlives the presenter; no pad owns this image.
class RootImagePresenter : public FramePresenter {
public:
    explicit RootImagePresenter(TCanvas& canvas);
    ~RootImagePresenter() override;
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
