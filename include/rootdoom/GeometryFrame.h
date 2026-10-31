// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace rootdoom {
struct GeometryTexture {
    std::uint64_t id = 0;
    int width = 0, height = 0, left = 0, top = 0;
    bool repeat = true;
    std::vector<unsigned char> rgba;
};
struct GeometryVertex { float x, y, z, u, v, light; };
struct GeometryBatch {
    std::shared_ptr<const GeometryTexture> texture;
    std::vector<GeometryVertex> vertices; // Independent triangles, never engine pointers.
    bool fullbright = false;
    float alpha = 1;
};
struct GeometryFrame {
    std::uint64_t sequence = 0, level = 0;
    bool world = false, full_ui = true;
    int map = 0, tic = 0, sprites = 0;
    float eye_x = 0, eye_y = 0, eye_z = 0, angle = 0;
    int view_x = 0, view_y = 0, view_width = 320, view_height = 168;
    std::vector<GeometryBatch> world_batches, weapon_batches;
    std::shared_ptr<const GeometryTexture> sky;
    std::vector<unsigned char> ui; // RGBA, full 640x400 UI or just the status bar.
    std::array<float, 4> tint{};
    std::size_t TriangleCount() const;
};
// One immutable pending scene. GUI keeps its own last displayed scene for expose.
class GeometryMailbox {
public:
    void Publish(std::shared_ptr<const GeometryFrame> frame);
    std::shared_ptr<const GeometryFrame> Consume();
private:
    std::mutex mutex_;
    std::shared_ptr<const GeometryFrame> pending_;
};
struct MapPoint { double x, y; };
using MapPolygon = std::vector<MapPoint>;
// Clip a convex polygon against the right/left side of a directed map line.
MapPolygon ClipMapPolygon(const MapPolygon& polygon, MapPoint origin, MapPoint direction, bool right);
} // namespace rootdoom
