#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// parity: `WorldBorder`.
class WorldBorder {
public:
    static constexpr double kDefaultSize = 5.9999968E7;

    /// parity: the caps `/worldborder set` refuses past.
    static constexpr double kMaxSize = 5.999997E7;
    static constexpr double kMaxCenterCoordinate = 2.9999984E7;

    std::int32_t warningBlocks = 5;
    std::int32_t warningSeconds = 15;
    double damageBuffer = 5.0;
    double damagePerBlock = 0.2;

    [[nodiscard]] double centerX() const noexcept { return centerX_; }

    [[nodiscard]] double centerZ() const noexcept { return centerZ_; }

    [[nodiscard]] double size() const noexcept { return size_; }

    [[nodiscard]] double targetSize() const noexcept { return targetSize_; }

    [[nodiscard]] std::int64_t lerpTimeTicks() const noexcept { return lerpTimeTicks_; }

    /// parity: `WorldBorder.setAbsoluteMaxSize`, fed by `max-world-size`.
    void setAbsoluteMaxSize(std::int32_t limit) noexcept { absoluteMaxSize_ = limit; }

    void center(double x, double z);

    void setSize(double newSize);

    /// parity: `WorldBorder.lerpSizeBetween` reports the old size while it moves and counts in ticks.
    void moveSize(double newSize, std::int64_t overTicks);

    [[nodiscard]] wire::protocol::play::InitializeBorderPacket initializePacket() const;

    /// parity: `getDistanceToBorder`.
    [[nodiscard]] double distanceOutside(double x, double z) const;

    /// parity: `ServerPlayer.tick`.
    [[nodiscard]] float damageAt(double x, double z) const;

    /// parity: `WorldBorder.isWithinBounds(double, double)`.
    [[nodiscard]] bool contains(double x, double z) const;

    void settle();

    /// parity: `WorldBorder.Settings`, as the `world_border` saved data keeps it.
    [[nodiscard]] wire::nbt::NbtCompound encode() const;

    void restore(const wire::nbt::NbtCompound& settings);

private:
    double centerX_ = 0.0;
    double centerZ_ = 0.0;
    double size_ = kDefaultSize;
    double targetSize_ = kDefaultSize;
    std::int64_t lerpTimeTicks_ = 0;
    std::int32_t absoluteMaxSize_ = wire::protocol::play::InitializeBorderPacket::kAbsoluteMaxSize;
};

} // namespace basalt::server::runtime::world
