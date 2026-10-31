// SPDX-License-Identifier: GPL-2.0-or-later
#include "rootdoom/GeometryFrame.h"
#include <cmath>
#include <utility>

namespace rootdoom {
std::size_t GeometryFrame::TriangleCount() const
{
    std::size_t count = 0;
    for (const auto& batch : world_batches) count += batch.vertices.size() / 3;
    return count;
}
void GeometryMailbox::Publish(std::shared_ptr<const GeometryFrame> frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pending_ = std::move(frame);
}
std::shared_ptr<const GeometryFrame> GeometryMailbox::Consume()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::exchange(pending_, {});
}
MapPolygon ClipMapPolygon(const MapPolygon& polygon, MapPoint origin, MapPoint direction, bool right)
{
    MapPolygon result;
    if (polygon.empty()) return result;
    auto distance = [&](MapPoint p) {
        const double cross = direction.x * (p.y - origin.y) - direction.y * (p.x - origin.x);
        return right ? -cross : cross;
    };
    auto previous = polygon.back();
    double previous_d = distance(previous);
    for (auto point : polygon) {
        const double current_d = distance(point);
        const bool inside = current_d >= -1e-7, previous_inside = previous_d >= -1e-7;
        if (inside != previous_inside) {
            const double t = previous_d / (previous_d - current_d);
            result.push_back({previous.x + t * (point.x - previous.x), previous.y + t * (point.y - previous.y)});
        }
        if (inside) result.push_back(point);
        previous = point;
        previous_d = current_d;
    }
    return result;
}
} // namespace rootdoom
