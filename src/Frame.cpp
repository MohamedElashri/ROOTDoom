// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Frame.h"

#include <array>

namespace rootdoom {

void MakeTestPattern(std::vector<std::uint32_t>& frame, std::uint64_t sequence)
{
    frame.resize(kFramePixels);
    constexpr std::array<std::uint32_t, 5> top{0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
    constexpr std::array<std::uint32_t, 5> bottom{0x00ffff, 0xff00ff, 0xffff00, 0x000000, 0xffffff};
    const unsigned bar = static_cast<unsigned>((sequence * 4) % kFrameWidth);
    for (unsigned y = 0; y < kFrameHeight; ++y) {
        for (unsigned x = 0; x < kFrameWidth; ++x) {
            std::uint32_t color;
            if (y < 80) {
                color = top[x / 128];
            } else if (y >= 320) {
                color = bottom[x / 128];
            } else {
                const bool light = ((x / 32 + y / 32 + sequence / 8) % 2) == 0;
                color = light ? 0x606060 : 0x202020;
                const int dx = static_cast<int>(x) - 320;
                const int dy = static_cast<int>(y) - 200;
                const int radius2 = dx * dx + dy * dy;
                if (radius2 >= 58 * 58 && radius2 <= 62 * 62) {
                    color = 0xffffff;
                }
                if ((x + kFrameWidth - bar) % kFrameWidth < 12) {
                    color = 0xffff00;
                }
            }
            frame[y * kFrameWidth + x] = color;
        }
    }
}

} // namespace rootdoom
