// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/RootImagePresenter.h"
#include <memory>
namespace rootdoom {
class GeometryMailbox;
// ROOT's embedded OpenGL viewer owns the graphics context and executes the scene.
class RootGeometryPresenter final : public FramePresenter {
public:
    RootGeometryPresenter(TCanvas& canvas, GeometryMailbox& mailbox, bool wireframe);
    ~RootGeometryPresenter() override;
    PresentationTimes Present(const std::vector<std::uint32_t>& rgb) override;
    void Capture(const std::string& path) override;
    unsigned ResizeCount() const override;
    int SceneObjectCount() const override { return 1; }
    GeometryCounts GeometryStatistics() const override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rootdoom
