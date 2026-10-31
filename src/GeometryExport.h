// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace rootdoom {
class GeometryMailbox;
// Worker-only view of the pinned C engine. Exported frames own all their data.
class GeometryExporter {
public:
    GeometryExporter(const std::string& iwad, GeometryMailbox& mailbox);
    ~GeometryExporter();
    void NewLevel();
    void Publish(const std::uint32_t* framebuffer);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rootdoom
