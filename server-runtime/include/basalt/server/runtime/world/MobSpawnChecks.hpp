#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/SpawnPlacementRules.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <span>

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Which of vanilla's mob classes a type is built from, as far as spawning asks.
enum class MobFamily : std::uint8_t {
    /// Not a `Mob` at all: an armor stand, a boat, an item.
    NotMob,
    /// A `Mob` that is not a `PathfinderMob`: bat, ghast, phantom, ender dragon.
    Mob,
    /// A `PathfinderMob` that is neither a monster nor an animal: golems, fish, villagers, slimes.
    Pathfinder,
    Monster,
    Animal,
};

/// The checks a spawner runs on a mob it has already made, before it lets it into the world.
///
/// parity: `Mob.checkSpawnRules` and `Mob.checkSpawnObstruction` with every override the mob
/// classes make of them, and of the `getWalkTargetValue` the first one reads.
namespace mobSpawnChecks {

/// parity: the class each type is constructed as, read off `EntityTypes`.
[[nodiscard]] MobFamily familyOf(const wire::common::Identifier& type);

[[nodiscard]] inline bool isMob(const wire::common::Identifier& type) { return familyOf(type) != MobFamily::NotMob; }

/// parity: `Mob.checkSpawnRules`: always for a plain `Mob`, and for a `PathfinderMob` a walk target
/// worth at least zero at the block it stands in. A fresh mob remembers nothing, so a hoglin has no
/// repellent to flee and a strider is not yet in lava.
[[nodiscard]] bool checkSpawnRules(LoadedWorld& world, const wire::common::Identifier& type, std::int32_t x,
                                   std::int32_t y, std::int32_t z, const SpawnRuleFacts& facts);

/// parity: `Mob.checkSpawnObstruction`. `box` is the mob's own bounding box at `x`, `y`, `z`, and
/// `players` the boxes of the players that could stand in its way.
[[nodiscard]] bool checkSpawnObstruction(LoadedWorld& world, const wire::common::Identifier& type,
                                         const entity::Aabb& box, double x, double y, double z,
                                         std::span<const entity::Aabb> players, const SpawnRuleFacts& facts);

/// parity: `CollisionGetter.noCollision(null, box)`: no block collision shape and no entity that can
/// be collided with overlaps the box.
[[nodiscard]] bool noCollision(LoadedWorld& world, const entity::Aabb& box);

/// parity: `LevelReader.containsAnyLiquid`, every block from the box's corner to its far corner.
[[nodiscard]] bool containsAnyLiquid(LoadedWorld& world, const entity::Aabb& box);

/// parity: `EntityGetter.isUnobstructed(entity)`: nothing that blocks building overlaps the box.
[[nodiscard]] bool isUnobstructed(LoadedWorld& world, const entity::Aabb& box, std::span<const entity::Aabb> players);

} // namespace mobSpawnChecks

} // namespace basalt::server::runtime::world
