// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace rootdoom {
// Accepts ROOT EKeySym values (not X11 raw keycodes). Zero means unsupported.
unsigned char DoomKey(std::uint32_t root_keysym);
} // namespace rootdoom
