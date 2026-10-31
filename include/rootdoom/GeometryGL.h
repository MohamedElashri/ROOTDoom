// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/GeometryFrame.h"
#include <memory>
namespace rootdoom {
// Runs only with the caller's GL context current. Used by ROOT's scene renderer
// and by offscreen pixel tests; it owns no window, event loop or engine state.
class GeometryGL {
public:
    GeometryGL();
    ~GeometryGL();
    void Draw(const GeometryFrame& frame, int width, int height, bool wireframe = false);
    void Release();
    std::size_t TextureCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rootdoom
