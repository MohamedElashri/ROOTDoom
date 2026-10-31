// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/Wad.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
void Write(const std::array<unsigned char, 28>& bytes)
{
    std::ofstream file("wad-structural-test.wad", std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
bool Rejected()
{
    try { rootdoom::ValidateWad("wad-structural-test.wad"); return false; }
    catch (const std::invalid_argument&) { return true; }
}
} // namespace
int main()
{
    try {
        std::array<unsigned char, 28> fixture{'I','W','A','D',1,0,0,0,12,0,0,0,
            0,0,0,0,0,0,0,0,'E','1','M','1',0,0,0,0};
        Write(fixture);
        const auto info = rootdoom::ValidateWad("wad-structural-test.wad");
        Require(info.lumps == 1 && info.bytes == 28 && std::filesystem::path(info.path).is_absolute(), "Structural IWAD parse failed");
        fixture[0] = 'P'; Write(fixture); Require(Rejected(), "PWAD accepted as IWAD");
        fixture[0] = 'I'; fixture[4] = 255; Write(fixture); Require(Rejected(), "Directory beyond EOF accepted");
        fixture[4] = 1; fixture[16] = 100; Write(fixture); Require(Rejected(), "Out-of-bounds lump accepted");
        fixture[16] = 0; fixture[20] = 'X'; Write(fixture); Require(Rejected(), "No Doom map accepted");
        std::filesystem::remove("wad-structural-test.wad");
        rootdoom::ValidateDoomArguments({"-warp", "1", "1", "-skill", "3", "-nomonsters"});
        rootdoom::ValidateDoomArguments({"-warp", "30", "-fast", "-respawn"});
        for (const auto& arguments : std::array<std::vector<std::string>, 7>{{
            {"-iwad", "other.wad"}, {"-gfxmode", "rgb565"}, {"-scaling", "100"},
            {"@response"}, {"-skill", "0"}, {"-warp"}, {"-config", "other.cfg"}}}) {
            bool rejected = false;
            try { rootdoom::ValidateDoomArguments(arguments); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected, "Unsafe or invalid engine argument accepted");
        }
        std::cout << "WAD structural validation and engine argument allowlist passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
