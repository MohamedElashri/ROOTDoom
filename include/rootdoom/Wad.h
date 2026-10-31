// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rootdoom {
struct WadInfo { std::string path; std::uint32_t lumps = 0; std::uint64_t bytes = 0; };
WadInfo ValidateWad(const std::string& path);
void ValidateDoomArguments(const std::vector<std::string>& arguments);
} // namespace rootdoom
