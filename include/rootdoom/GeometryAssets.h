// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rootdoom/GeometryFrame.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rootdoom {
using DoomPalette = std::array<std::array<unsigned char, 3>, 256>;
GeometryTexture DecodeDoomPatch(const std::vector<unsigned char>& data, const DoomPalette& palette);
// Worker-only asset reader. Parses the original IWAD; no preprocessing or extracted assets.
class GeometryAssets {
public:
    explicit GeometryAssets(const std::string& iwad);
    std::shared_ptr<const GeometryTexture> Wall(int index);
    std::shared_ptr<const GeometryTexture> Flat(int lump);
    std::shared_ptr<const GeometryTexture> Patch(int lump);
    int Find(const std::string& name) const;
    int WallCount() const { return static_cast<int>(walls_.size()); }
private:
    struct Lump { std::uint32_t offset, size; std::string name; };
    struct PatchPlacement { int x, y, lump; };
    struct WallRecipe { int width, height; std::vector<PatchPlacement> patches; };
    std::vector<unsigned char> LumpBytes(int lump) const;
    std::shared_ptr<const GeometryTexture> Store(GeometryTexture texture, int key);
    std::vector<unsigned char> wad_;
    std::vector<Lump> lumps_;
    std::vector<WallRecipe> walls_;
    std::unordered_map<std::string, int> names_;
    std::unordered_map<int, std::shared_ptr<const GeometryTexture>> cache_;
    DoomPalette palette_{};
    std::uint64_t next_id_ = 1;
};
} // namespace rootdoom
