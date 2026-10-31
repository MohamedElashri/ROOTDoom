// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Frame.h"
#include "rootdoom/RootImagePresenter.h"

#include <TApplication.h>
#include <TASImage.h>
#include <TCanvas.h>
#include <TROOT.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const std::string& reason)
{
    if (!condition) {
        throw std::runtime_error(reason);
    }
}

std::uint32_t ReadPixel(TASImage& image, unsigned x, unsigned y)
{
    auto* pixels = image.GetArgbArray();
    Require(pixels != nullptr && x < image.GetWidth() && y < image.GetHeight(), "Invalid captured pixels");
    return pixels[y * image.GetWidth() + x];
}

void CheckSwatches(TASImage& saved)
{
    // Expected image rectangle is computed in screenshot pixel coordinates.
    const double scale = std::min(saved.GetWidth() / 640., saved.GetHeight() / 400.);
    const double x0 = (saved.GetWidth() - 640 * scale) / 2;
    const double y0 = (saved.GetHeight() - 400 * scale) / 2;
    constexpr std::array<std::uint32_t, 5> top{0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff, 0xff000000};
    constexpr std::array<std::uint32_t, 5> bottom{0xff00ffff, 0xffff00ff, 0xffffff00, 0xff000000, 0xffffffff};
    for (unsigned i = 0; i < top.size(); ++i) {
        const unsigned x = static_cast<unsigned>(x0 + (i * 128 + 64) * scale);
        Require(ReadPixel(saved, x, static_cast<unsigned>(y0 + 40 * scale)) == top[i],
                "Top swatch channel/alpha/orientation mismatch at " + std::to_string(i));
        Require(ReadPixel(saved, x, static_cast<unsigned>(y0 + 360 * scale)) == bottom[i],
                "Bottom swatch channel/alpha/orientation mismatch at " + std::to_string(i));
    }
    if (x0 > 4 || y0 > 4) {
        Require(ReadPixel(saved, 1, 1) == 0xff000000U, "Letterbox is not black");
    }
}
} // namespace

int main(int argc, char** argv)
{
    (void)argc;
    char batch[] = "-b";
    int root_argc = 2;
    char* root_argv[] = {argv[0], batch, nullptr};
    TApplication application("PresenterTest", &root_argc, root_argv);
    Require(gROOT->IsBatch(), "Presenter test must be batch-only");
    try {
        static_assert(rootdoom::OpaqueArgb(0x00112233) == 0xff112233);
        static_assert(rootdoom::OpaqueArgb(0x77112233) == 0xff112233);
        TCanvas canvas("presenter_test", "ROOTDoom pixel test", 640, 400);
        rootdoom::RootImagePresenter presenter(canvas);
        std::vector<std::uint32_t> frame;
        constexpr std::array<std::array<unsigned, 2>, 4> sizes{{{640, 400}, {1280, 800}, {800, 800}, {1000, 300}}};
        int objects = -1;
        for (const auto& size : sizes) {
            canvas.SetCanvasSize(size[0], size[1]);
            rootdoom::MakeTestPattern(frame, 0);
            presenter.Present(frame);
            presenter.Capture("presenter-pattern.png");
            TASImage captured("presenter-pattern.png");
            Require(captured.GetWidth() == size[0] && captured.GetHeight() == size[1], "Capture geometry differs from canvas");
            CheckSwatches(captured);
            if (objects < 0) {
                objects = presenter.SceneObjectCount();
            }
            // Repainting same-size scaled images must replace the old frame.
            for (unsigned i = 0; i < 12; ++i) {
                std::fill(frame.begin(), frame.end(), (i % 2) ? 0x00ff0000U : 0x000000ffU);
                presenter.Present(frame);
                Require(presenter.SceneObjectCount() == objects, "ROOT scene grows across frames/resizes");
            }
            presenter.Capture("presenter-latest.png");
            TASImage latest("presenter-latest.png");
            Require(ReadPixel(latest, latest.GetWidth() / 2, latest.GetHeight() / 2) == 0xffff0000U,
                    "Presenter displayed a stale scaled frame");
        }
        Require(presenter.ResizeCount() == 3, "Resize tracking mismatch");
        bool rejected = false;
        try {
            presenter.Present(std::vector<std::uint32_t>(1));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "Invalid frame size accepted");
        std::cout << "Presenter: RGB/alpha/orientation, four sizes, latest frame, bounded scene and validation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Presenter test failed: " << error.what() << '\n';
        return 1;
    }
}
