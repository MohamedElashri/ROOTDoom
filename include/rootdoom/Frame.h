// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rootdoom {

constexpr unsigned kFrameWidth = 640;
constexpr unsigned kFrameHeight = 400;
constexpr std::size_t kFramePixels = kFrameWidth * kFrameHeight;

constexpr std::uint32_t OpaqueArgb(std::uint32_t rgb) { return rgb | 0xff000000U; }

// Pixel row zero is the top of the image. Input alpha is intentionally absent.
void MakeTestPattern(std::vector<std::uint32_t>& frame, std::uint64_t sequence);

} // namespace rootdoom
