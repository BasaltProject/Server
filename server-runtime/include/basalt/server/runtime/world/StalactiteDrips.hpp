#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `PointedDripstoneBlock.maybeTransferFluid` and `AbstractCauldronBlock.tick`.
namespace stalactiteDrips {

inline constexpr std::int32_t kDripEvent = 1504;
inline constexpr std::int32_t kWaterIntoCauldron = 1047;
inline constexpr std::int32_t kLavaIntoCauldron = 1046;

using Position = std::array<std::int32_t, 3>;

struct Transfer {
    Position tip{};
    std::optional<Position> clay;
    std::optional<Position> cauldron;
    std::int32_t delay = 0;
};

/// `evaporates` is the dimension's `water_evaporates`, which dries the mud's water.
[[nodiscard]] std::optional<Transfer> transfer(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               std::int32_t stateId, float randomValue, bool evaporates);

struct Fill {
    std::int32_t stateId = 0;
    std::int32_t levelEvent = 0;
};

[[nodiscard]] std::optional<Fill> fillCauldron(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               bool evaporates);

} // namespace stalactiteDrips

} // namespace basalt::server::runtime::world
