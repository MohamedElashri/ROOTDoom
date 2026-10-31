// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Wad.h"
#include <array>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace rootdoom {
namespace {
std::uint32_t Little32(const unsigned char* p)
{ return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24); }
bool Number(const std::string& value, unsigned max)
{
    unsigned number = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && number >= 1 && number <= max;
}
} // namespace

WadInfo ValidateWad(const std::string& path)
{
    if (path.empty()) throw std::invalid_argument("An IWAD is required: use --iwad /path/to/doom1.wad");
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    if (!std::filesystem::is_regular_file(absolute)) throw std::invalid_argument("IWAD is not a readable regular file: " + absolute.string());
    const auto size = std::filesystem::file_size(absolute);
    if (size < 12 || size > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::invalid_argument("IWAD size is invalid (expected a classic Doom IWAD smaller than 2 GiB)");
    }
    std::ifstream file(absolute, std::ios::binary);
    std::array<unsigned char, 16> entry{};
    if (!file.read(reinterpret_cast<char*>(entry.data()), 12) || std::memcmp(entry.data(), "IWAD", 4) != 0) {
        throw std::invalid_argument("Invalid IWAD header; PWADs/mods are not standalone game data");
    }
    const auto count = Little32(entry.data() + 4);
    const auto directory = Little32(entry.data() + 8);
    if (!count || count > 1000000 || directory < 12 || std::uint64_t(directory) + std::uint64_t(count) * 16 > size) {
        throw std::invalid_argument("Invalid/truncated IWAD lump directory");
    }
    file.seekg(directory);
    bool has_map = false;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (!file.read(reinterpret_cast<char*>(entry.data()), entry.size())) throw std::invalid_argument("Truncated IWAD directory");
        if (std::uint64_t(Little32(entry.data())) + Little32(entry.data() + 4) > size) {
            throw std::invalid_argument("IWAD lump extends beyond the end of the file");
        }
        const std::string name(reinterpret_cast<char*>(entry.data() + 8), 8);
        has_map = has_map || name.compare(0, 4, "E1M1") == 0 || name.compare(0, 5, "MAP01") == 0;
    }
    if (!has_map) throw std::invalid_argument("IWAD contains no initial Doom map (E1M1 or MAP01)");
    return {absolute.string(), count, size};
}

void ValidateDoomArguments(const std::vector<std::string>& arguments)
{
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto& flag = arguments[i];
        if (flag == "-nomonsters" || flag == "-fast" || flag == "-respawn" || flag == "-devparm") continue;
        if (flag == "-skill") {
            if (++i >= arguments.size() || !Number(arguments[i], 5)) throw std::invalid_argument("-skill requires 1..5");
        } else if (flag == "-warp") {
            if (++i >= arguments.size() || !Number(arguments[i], 99)) throw std::invalid_argument("-warp requires a map number, or episode and map");
            if (i + 1 < arguments.size() && Number(arguments[i + 1], 99)) ++i;
        } else {
            throw std::invalid_argument("Unsupported Doom argument: " + flag + "; allowed: -warp, -skill, -nomonsters, -fast, -respawn, -devparm");
        }
    }
}
} // namespace rootdoom
