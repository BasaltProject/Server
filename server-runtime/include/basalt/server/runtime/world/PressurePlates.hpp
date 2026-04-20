#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `BasePressurePlateBlock.checkPressed`.
namespace pressurePlates {

/// parity: `getPressedTime`, twenty ticks for a plain plate and ten for a weighted one.
inline constexpr std::int32_t kPressedTicks = 20;
inline constexpr std::int32_t kWeightedPressedTicks = 10;

inline constexpr std::int32_t kLightMaxWeight = 15;
inline constexpr std::int32_t kHeavyMaxWeight = 150;

[[nodiscard]] bool isPlate(std::int32_t stateId);

/// parity: `entityInside`.
enum class Trigger : std::uint8_t {
    Touched,
    Scheduled,
};

/// A pressed plate books its own turn, which checks whether whatever pressed it has gone.
[[nodiscard]] std::optional<BlockChange> update(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                Trigger from, std::int32_t standing, std::int64_t gameTime = 0);

/// parity: `BasePressurePlateBlock`.
[[nodiscard]] bool touches(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
                           double entityZ, double halfWidth, double height);

[[nodiscard]] bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
                                double entityZ);

[[nodiscard]] std::int32_t blockUnder(double entityY);

} // namespace pressurePlates

} // namespace basalt::server::runtime::world
