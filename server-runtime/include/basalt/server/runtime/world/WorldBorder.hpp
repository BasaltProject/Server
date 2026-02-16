#pragma once

#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// One world's border. parity: `WorldBorder`, minus the damage it deals outside itself, which needs
/// the per-tick player damage pass; the numbers are kept so `/worldborder damage` still reports and
/// persists what an operator set.
class WorldBorder {
public:
    /// parity: `WorldBorder.DEFAULT_SIZE`.
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

    /// Where the size is now, and where it is heading if a move is in progress.
    [[nodiscard]] double size() const noexcept { return size_; }

    [[nodiscard]] double targetSize() const noexcept { return targetSize_; }

    [[nodiscard]] std::int64_t lerpTimeTicks() const noexcept { return lerpTimeTicks_; }

    void center(double x, double z);

    void setSize(double newSize);

    /// parity: `WorldBorder.lerpSizeBetween`, which reports the old size while it moves, and counts
    /// in ticks. It used to be told milliseconds here, which made every moving border crawl at a
    /// fiftieth of the speed the command asked for.
    void moveSize(double newSize, std::int64_t overTicks);

    [[nodiscard]] wire::protocol::play::InitializeBorderPacket initializePacket() const;

    /// How far outside the border a position is, in blocks, or 0 while it is inside.
    ///
    /// parity: `WorldBorder.getDistanceToBorder`, negated: vanilla measures towards the border and
    /// damages on the distance past it plus the buffer.
    [[nodiscard]] double distanceOutside(double x, double z) const;

    /// The damage one tick outside the border costs, or 0 inside it or inside the buffer.
    ///
    /// parity: `ServerPlayer.tick`'s border pass, which damages every ten ticks by
    /// `damagePerBlock * (distance - buffer)`, with a floor of the per-block amount.
    [[nodiscard]] float damageAt(double x, double z) const;

    /// Whether a square is inside the border at all.
    ///
    /// parity: `WorldBorder.isWithinBounds(double, double)`, which is closed on the low side and
    /// open on the high one.
    [[nodiscard]] bool contains(double x, double z) const;

    /// Called when a move finishes, so the stored size stops being the one it started from.
    void settle();

private:
    double centerX_ = 0.0;
    double centerZ_ = 0.0;
    double size_ = kDefaultSize;
    double targetSize_ = kDefaultSize;
    std::int64_t lerpTimeTicks_ = 0;
};

} // namespace basalt::server::runtime::world
