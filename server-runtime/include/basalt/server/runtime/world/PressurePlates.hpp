#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The plates that answer to whatever is standing on them.
///
/// parity: `BasePressurePlateBlock.checkPressed`: standing on one presses it and books a turn, and
/// that turn is what lets it up again once nothing is there. A wooden or stone plate is fifteen or
/// nothing; a weighted one counts what is on it against its own limit.
///
/// What counts as standing on it is the box vanilla uses, which is narrower than the block:
/// fourteen sixteenths across and four tall, so brushing past the edge does not press it.
namespace pressurePlates {

/// parity: `getPressedTime`, twenty ticks for a plain plate and ten for a weighted one.
inline constexpr std::int32_t kPressedTicks = 20;
inline constexpr std::int32_t kWeightedPressedTicks = 10;

/// parity: the weight each weighted plate maxes out at.
inline constexpr std::int32_t kLightMaxWeight = 15;
inline constexpr std::int32_t kHeavyMaxWeight = 150;

[[nodiscard]] bool isPlate(std::int32_t stateId);

/// Which of the two halves of `BasePressurePlateBlock` is asking.
///
/// parity: they are not the same question. `entityInside` recomputes **only while the plate reads
/// zero**, so once it is on it stops looking; `tick` recomputes **only while it reads more than
/// zero**, which is the turn that lets it up again. A plate that recomputed on both would never show
/// the half-count a plate really shows when several entities land on the same tick.
enum class Trigger : std::uint8_t {
    /// Something is inside the plate's box this tick.
    Touched,
    /// The turn the plate booked for itself.
    Scheduled,
};

/// Presses or releases the plate at these coordinates, given how many things are standing on it.
///
/// Answers the change, or nothing when it was already showing that. A pressed plate books its own
/// turn, which is what checks whether whatever pressed it has gone.
[[nodiscard]] std::optional<BlockChange> update(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                Trigger from, std::int32_t standing, std::int64_t gameTime = 0);

/// Whether this position is inside the box that presses the plate at these coordinates.
[[nodiscard]] bool isStandingOn(std::int32_t x, std::int32_t y, std::int32_t z, double entityX, double entityY,
                                double entityZ);

/// Which block a thing at this height is standing on: the one under its feet.
[[nodiscard]] std::int32_t blockUnder(double entityY);

} // namespace pressurePlates

} // namespace basalt::server::runtime::world
