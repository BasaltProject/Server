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

wire::nbt::NbtCompound WorldBorder::encode() const {
    constexpr double kTicksPerSecond = 20.0;
    wire::nbt::NbtCompound out;
    out.putDouble("center_x", centerX_);
    out.putDouble("center_z", centerZ_);
    out.putDouble("damage_per_block", damagePerBlock);
    out.putDouble("safe_zone", damageBuffer);
    out.putInt("warning_blocks", warningBlocks);
    out.putInt("warning_time", static_cast<std::int32_t>(warningSeconds * kTicksPerSecond));
    out.putDouble("size", size_);
    out.putLong("lerp_time", lerpTimeTicks_);
    out.putDouble("lerp_target", targetSize_);
    return out;
}

void WorldBorder::restore(const wire::nbt::NbtCompound& settings) {
    constexpr double kTicksPerSecond = 20.0;
    center(settings.getDouble("center_x"), settings.getDouble("center_z"));
    damagePerBlock = settings.getDouble("damage_per_block", 0.2);
    damageBuffer = settings.getDouble("safe_zone", 5.0);
    warningBlocks = settings.getInt("warning_blocks", 5);
    warningSeconds = static_cast<std::int32_t>(settings.getInt("warning_time", 300) / kTicksPerSecond);
    setSize(settings.getDouble("size", kDefaultSize));
    if (const std::int64_t lerp = settings.getLong("lerp_time"); lerp > 0) {
        moveSize(settings.getDouble("lerp_target", size_), lerp);
    }
}

void WorldBorder::settle() {
    size_ = targetSize_;
    lerpTimeTicks_ = 0;
}

} // namespace basalt::server::runtime::world
