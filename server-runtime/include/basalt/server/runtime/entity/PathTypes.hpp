#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

namespace pathTypes {

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

[[nodiscard]] const Maluses& defaults();

[[nodiscard]] Maluses of(const wire::common::Identifier& type);

[[nodiscard]] inline float malusOf(const Maluses& maluses, PathType type) {
    return maluses[static_cast<std::size_t>(type)];
}

[[nodiscard]] bool isPathfindableOnLand(std::int32_t stateId);

[[nodiscard]] PathType fromState(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] PathType checkNeighbours(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                       PathType type);

[[nodiscard]] PathType flying(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                              std::int32_t minY, const std::optional<std::array<std::int32_t, 3>>& mobAt);

} // namespace pathTypes

} // namespace basalt::server::runtime::entity
