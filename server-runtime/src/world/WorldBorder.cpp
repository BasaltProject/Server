#include <basalt/server/runtime/world/WorldBorder.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world {

void WorldBorder::center(double x, double z) {
    centerX_ = x;
    centerZ_ = z;
}

void WorldBorder::setSize(double newSize) {
    size_ = newSize;
    targetSize_ = newSize;
    lerpTimeTicks_ = 0;
}

void WorldBorder::moveSize(double newSize, std::int64_t overTicks) {
    targetSize_ = newSize;
    lerpTimeTicks_ = overTicks;
}

wire::protocol::play::InitializeBorderPacket WorldBorder::initializePacket() const {
    return wire::protocol::play::InitializeBorderPacket{
        .centerX = centerX_,
        .centerZ = centerZ_,
        .oldSize = size_,
        .newSize = targetSize_,
        .lerpTimeTicks = lerpTimeTicks_,
        .absoluteMaxSize = wire::protocol::play::InitializeBorderPacket::kAbsoluteMaxSize,
        .warningBlocks = warningBlocks,
        .warningSeconds = warningSeconds};
}

double WorldBorder::distanceOutside(double x, double z) const {
    const double half = targetSize_ / 2.0;
    const double outsideX = std::max(std::abs(x - centerX_) - half, 0.0);
    const double outsideZ = std::max(std::abs(z - centerZ_) - half, 0.0);
    return std::max(outsideX, outsideZ);
}

bool WorldBorder::contains(double x, double z) const {
    // parity: `isWithinBounds(x, z, 0)`.
    const double half = size_ / 2.0;
    return x >= centerX_ - half && x < centerX_ + half && z >= centerZ_ - half && z < centerZ_ + half;
}

float WorldBorder::damageAt(double x, double z) const {
    const double outside = distanceOutside(x, z) - damageBuffer;
    if (outside <= 0.0) {
        return 0.0F;
    }
    return static_cast<float>(std::max(1.0, outside * damagePerBlock));
}

void WorldBorder::settle() {
    size_ = targetSize_;
    lerpTimeTicks_ = 0;
}

} // namespace basalt::server::runtime::world
