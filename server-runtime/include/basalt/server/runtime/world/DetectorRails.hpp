#pragma once

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace basalt::server::runtime::world {

/// parity: `DetectorRailBlock.checkPressed`.
namespace detectorRails {
/// parity: `EntityTypes.MINECART.sized(0.98F, 0.7F)`.
inline constexpr double kCartHalfWidth = 0.49;

/// The other half of `EntityTypes.MINECART.sized(0.98F, 0.7F)`: how tall a cart's box is.
inline constexpr double kCartHeight = 0.7;

inline constexpr std::int32_t kRecheckTicks = 20;

/// parity: `AbstractMinecart`, of which any kind presses the rail.
[[nodiscard]] bool isMinecart(const wire::common::Identifier& type);

struct Checked {
    std::optional<BlockChange> change;
    /// parity: `checkPressed`.
    bool ran = false;
};

enum class Caller : std::uint8_t {
    /// parity: `entityInside`, which asks only while the rail is **not** powered.
    Entity,
    /// parity: `tick`, which asks only while it **is**.
    ScheduledTick,
};

/// parity: `checkPressed`.
[[nodiscard]] Checked check(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                            std::span<entity::ServerEntity* const> entities, Caller caller);

/// parity: `DetectorRailBlock.getAnalogOutputSignal`.
[[nodiscard]] std::int32_t comparatorSignal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::span<entity::ServerEntity* const> entities);

} // namespace detectorRails

} // namespace basalt::server::runtime::world
