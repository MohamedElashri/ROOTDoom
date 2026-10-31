// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/GeometryAssets.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace rootdoom {
namespace {
void Bounds(const std::vector<unsigned char>& bytes, std::size_t at, std::size_t size)
{
    if (at > bytes.size() || size > bytes.size() - at) throw std::runtime_error("Truncated Doom graphics lump");
}
unsigned U16(const std::vector<unsigned char>& b, std::size_t at)
{ Bounds(b, at, 2); return b[at] | (unsigned(b[at + 1]) << 8); }
int I16(const std::vector<unsigned char>& b, std::size_t at) { return static_cast<std::int16_t>(U16(b, at)); }
std::uint32_t U32(const std::vector<unsigned char>& b, std::size_t at)
{ Bounds(b, at, 4); return b[at] | (std::uint32_t(b[at+1])<<8) | (std::uint32_t(b[at+2])<<16) | (std::uint32_t(b[at+3])<<24); }
std::string Name(const std::vector<unsigned char>& b, std::size_t at)
{
    Bounds(b, at, 8);
    std::string name;
    for (unsigned i = 0; i < 8 && b[at+i]; ++i) {
        char c = static_cast<char>(b[at+i]);
        name += c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }
    return name;
}
void Allocate(GeometryTexture& texture, int width, int height)
{
    if (width < 1 || height < 1 || width > 4096 || height > 4096) throw std::runtime_error("Invalid Doom texture dimensions");
    texture.width = width; texture.height = height;
    texture.rgba.resize(std::size_t(width) * height * 4);
}
} // namespace
GeometryTexture DecodeDoomPatch(const std::vector<unsigned char>& data, const DoomPalette& palette)
{
    GeometryTexture texture;
    Allocate(texture, U16(data, 0), U16(data, 2));
    texture.left = I16(data, 4); texture.top = I16(data, 6); texture.repeat = false;
    Bounds(data, 8, std::size_t(texture.width) * 4);
    for (int x = 0; x < texture.width; ++x) {
        std::size_t at = U32(data, 8 + std::size_t(x) * 4);
        int previous_top = -1;
        for (;;) {
            Bounds(data, at, 1);
            if (data[at] == 255) break;
            Bounds(data, at, 3);
            int top = data[at];
            if (top <= previous_top) top += previous_top; // Tall-patch relative posts.
            previous_top = top;
            const unsigned count = data[at + 1];
            Bounds(data, at, std::size_t(count) + 4);
            for (unsigned y = 0; y < count; ++y) {
                const int row = top + static_cast<int>(y);
                if (row >= texture.height) continue;
                auto* out = &texture.rgba[(std::size_t(row) * texture.width + x) * 4];
                const auto& color = palette[data[at + 3 + y]];
                std::copy(color.begin(), color.end(), out); out[3] = 255;
            }
            at += count + 4;
        }
    }
    return texture;
}
GeometryAssets::GeometryAssets(const std::string& iwad)
{
    std::ifstream file(iwad, std::ios::binary | std::ios::ate);
    const auto length = file.tellg();
    if (!file || length < 12 || length > 512 * 1024 * 1024) throw std::runtime_error("Geometry renderer requires an IWAD under 512 MiB");
    wad_.resize(static_cast<std::size_t>(length)); file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(wad_.data()), length) || std::memcmp(wad_.data(), "IWAD", 4)) throw std::runtime_error("Cannot read geometry IWAD");
    const auto count = U32(wad_, 4), directory = U32(wad_, 8);
    if (count > 1000000) throw std::runtime_error("Invalid graphics lump count");
    Bounds(wad_, directory, std::size_t(count) * 16);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t at = directory + std::size_t(i) * 16;
        Lump lump{U32(wad_, at), U32(wad_, at+4), Name(wad_, at+8)};
        Bounds(wad_, lump.offset, lump.size);
        names_[lump.name] = static_cast<int>(lumps_.size()); lumps_.push_back(std::move(lump));
    }
    const auto palette = LumpBytes(Find("PLAYPAL")); Bounds(palette, 0, 768);
    for (unsigned i = 0; i < 256; ++i) std::copy_n(palette.data() + i * 3, 3, palette_[i].data());
    const auto pnames = LumpBytes(Find("PNAMES")); const auto patches = U32(pnames, 0);
    if (patches > 100000) throw std::runtime_error("Invalid PNAMES count");
    Bounds(pnames, 4, std::size_t(patches) * 8);
    std::vector<int> patch_lumps;
    for (std::uint32_t i = 0; i < patches; ++i) patch_lumps.push_back(Find(Name(pnames, 4 + i * 8)));
    for (const char* name : {"TEXTURE1", "TEXTURE2"}) {
        const int lump = Find(name); if (lump < 0) continue;
        const auto data = LumpBytes(lump); const auto textures = U32(data, 0);
        if (textures > 100000) throw std::runtime_error("Invalid Doom texture count");
        Bounds(data, 4, std::size_t(textures) * 4);
        for (std::uint32_t i = 0; i < textures; ++i) {
            const auto at = U32(data, 4 + i * 4); Bounds(data, at, 22);
            WallRecipe wall{int(U16(data, at+12)), int(U16(data, at+14)), {}};
            const unsigned n = U16(data, at+20); Bounds(data, std::size_t(at)+22, n*10);
            for (unsigned j = 0; j < n; ++j) {
                const std::size_t p = std::size_t(at)+22+j*10;
                const unsigned index = U16(data, p+4);
                if (index >= patch_lumps.size()) throw std::runtime_error("Doom texture patch index out of range");
                wall.patches.push_back({I16(data,p), I16(data,p+2), patch_lumps[index]});
            }
            walls_.push_back(std::move(wall));
        }
    }
}
int GeometryAssets::Find(const std::string& name) const
{ const auto it = names_.find(name); return it == names_.end() ? -1 : it->second; }
std::vector<unsigned char> GeometryAssets::LumpBytes(int lump) const
{
    if (lump < 0 || std::size_t(lump) >= lumps_.size()) throw std::runtime_error("Missing Doom graphics lump");
    const auto& l = lumps_[lump];
    return {wad_.begin()+l.offset, wad_.begin()+l.offset+l.size};
}
std::shared_ptr<const GeometryTexture> GeometryAssets::Store(GeometryTexture texture, int key)
{
    texture.id = next_id_++;
    auto result = std::make_shared<const GeometryTexture>(std::move(texture));
    cache_[key] = result; return result;
}
std::shared_ptr<const GeometryTexture> GeometryAssets::Patch(int lump)
{
    const int key = lump * 3;
    const auto found = cache_.find(key); if (found != cache_.end()) return found->second;
    return Store(DecodeDoomPatch(LumpBytes(lump), palette_), key);
}
std::shared_ptr<const GeometryTexture> GeometryAssets::Flat(int lump)
{
    const int key = lump * 3 + 1;
    const auto found = cache_.find(key); if (found != cache_.end()) return found->second;
    const auto data = LumpBytes(lump); Bounds(data, 0, 4096);
    GeometryTexture texture; Allocate(texture, 64, 64);
    for (unsigned i = 0; i < 4096; ++i) {
        std::copy(palette_[data[i]].begin(), palette_[data[i]].end(), texture.rgba.data()+i*4); texture.rgba[i*4+3]=255;
    }
    return Store(std::move(texture), key);
}
std::shared_ptr<const GeometryTexture> GeometryAssets::Wall(int index)
{
    if (index < 0 || std::size_t(index) >= walls_.size()) throw std::runtime_error("Doom wall texture index out of range");
    const int key = index * 3 + 2;
    const auto found = cache_.find(key); if (found != cache_.end()) return found->second;
    const auto& recipe = walls_[index]; GeometryTexture texture; Allocate(texture, recipe.width, recipe.height);
    for (const auto& placement : recipe.patches) {
        const auto patch = Patch(placement.lump);
        for (int y = 0; y < patch->height; ++y) for (int x = 0; x < patch->width; ++x) {
            const int dx = x + placement.x, dy = y + placement.y;
            if (dx < 0 || dx >= texture.width || dy < 0 || dy >= texture.height) continue;
            const auto* in = patch->rgba.data() + (std::size_t(y) * patch->width + x) * 4;
            if (in[3]) std::copy_n(in, 4, texture.rgba.data() + (std::size_t(dy) * texture.width + dx) * 4);
        }
    }
    return Store(std::move(texture), key);
}
} // namespace rootdoom
