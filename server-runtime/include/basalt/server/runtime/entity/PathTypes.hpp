#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

/// What a square is to something finding its way through it.
///
/// parity: `PathType` and the three questions `WalkNodeEvaluator` and `FlyNodeEvaluator` ask of a
/// block: what it is itself, what standing over another makes it, and what burns or cuts nearby.
namespace pathTypes {

/// parity: `PathType`, in its id order.
enum class PathType : std::uint8_t {
    Blocked,
    Open,
    Walkable,
    WalkableDoor,
    Trapdoor,
    PowderSnow,
    OnTopOfPowderSnow,
    Fence,
    Lava,
    Water,
    WaterBorder,
    Rail,
    UnpassableRail,
    FireInNeighbor,
    Fire,
    DamagingInNeighbor,
    Damaging,
    DoorOpen,
    DoorWoodClosed,
    DoorIronClosed,
    Breach,
    Leaves,
    StickyHoney,
    Cocoa,
    DamageCautious,
    OnTopOfTrapdoor,
    BigMobsCloseToDanger,
};

inline constexpr std::size_t kCount = 27;

/// A mob's cost for each type. parity: `Mob.getPathfindingMalus`, the type's own unless it set one.
using Maluses = std::array<float, kCount>;

/// parity: `PathType.getMalus`.
[[nodiscard]] const Maluses& defaults();

/// parity: the `setPathfindingMalus` calls in each mob's constructor.
[[nodiscard]] Maluses of(const wire::common::Identifier& type);

[[nodiscard]] inline float malusOf(const Maluses& maluses, PathType type) {
    return maluses[static_cast<std::size_t>(type)];
}

/// parity: `WalkNodeEvaluator.getPathTypeFromState`, the block on its own.
[[nodiscard]] PathType fromState(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `WalkNodeEvaluator.checkNeighbourBlocks`: fire, lava, water or something that cuts in
/// the eight around it and the rows above and below.
[[nodiscard]] PathType checkNeighbours(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                       PathType type);

/// parity: `FlyNodeEvaluator.getPathType`: what is below changes an open square. `mobAt` is the
/// block the mob stands in, over which a fence is let through.
[[nodiscard]] PathType flying(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                              std::int32_t minY, const std::optional<std::array<std::int32_t, 3>>& mobAt);

} // namespace pathTypes

} // namespace basalt::server::runtime::entity
