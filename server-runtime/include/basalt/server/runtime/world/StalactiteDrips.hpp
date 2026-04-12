#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// A stalactite dripping what is above it: mud dried to clay, or a cauldron under it filled.
///
/// parity: `PointedDripstoneBlock.maybeTransferFluid` and `AbstractCauldronBlock.tick`.
namespace stalactiteDrips {

/// parity: `LevelEvent.DRIPSTONE_DRIP`, `CAULDRON_DRIP_WATER` and `CAULDRON_DRIP_LAVA`.
inline constexpr std::int32_t kDripEvent = 1504;
inline constexpr std::int32_t kWaterIntoCauldron = 1047;
inline constexpr std::int32_t kLavaIntoCauldron = 1046;

using Position = std::array<std::int32_t, 3>;

/// What one drip does. The tip is where the drip event plays.
struct Transfer {
    Position tip{};
    /// The mud above that turns to clay.
    std::optional<Position> clay;
    /// The cauldron below that is booked a turn, `delay` ticks away.
    std::optional<Position> cauldron;
    std::int32_t delay = 0;
};

/// The drip a pointed dripstone's random tick makes with the float it drew first, or nothing.
/// `evaporates` is the dimension's `water_evaporates`, which dries the mud's water.
[[nodiscard]] std::optional<Transfer> transfer(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, float randomValue, bool evaporates);

/// A cauldron's booked turn: the state it fills to and the event it plays, or nothing.
struct Fill {
    std::int32_t stateId = 0;
    std::int32_t levelEvent = 0;
};

[[nodiscard]] std::optional<Fill> fillCauldron(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               bool evaporates);

} // namespace stalactiteDrips

} // namespace basalt::server::runtime::world
