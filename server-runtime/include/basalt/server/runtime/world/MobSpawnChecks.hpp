#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/world/SpawnPlacementRules.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <span>

namespace basalt::server::runtime::world {

class LoadedWorld;

enum class MobFamily : std::uint8_t {
    NotMob,
    Mob,
    Pathfinder,
    Monster,
    Animal,
};

/// parity: `Mob.checkSpawnRules`.
namespace mobSpawnChecks {

/// parity: the class each type is constructed as, read off `EntityTypes`.
[[nodiscard]] MobFamily familyOf(const wire::common::Identifier& type);

[[nodiscard]] inline bool isMob(const wire::common::Identifier& type) { return familyOf(type) != MobFamily::NotMob; }

/// parity: `Mob.checkSpawnRules`.
[[nodiscard]] bool checkSpawnRules(LoadedWorld& world, const wire::common::Identifier& type, std::int32_t x,
                                   std::int32_t y, std::int32_t z, const SpawnRuleFacts& facts);

/// parity: `Mob.checkSpawnObstruction`.
[[nodiscard]] bool checkSpawnObstruction(LoadedWorld& world, const wire::common::Identifier& type,
                                         const entity::Aabb& box, double x, double y, double z,
                                         std::span<const entity::Aabb> players, const SpawnRuleFacts& facts);

/// parity: `CollisionGetter.noCollision(null, box)`.
[[nodiscard]] bool noCollision(LoadedWorld& world, const entity::Aabb& box);

/// parity: `LevelReader.containsAnyLiquid`.
[[nodiscard]] bool containsAnyLiquid(LoadedWorld& world, const entity::Aabb& box);

/// parity: `EntityGetter.isUnobstructed(entity)`.
[[nodiscard]] bool isUnobstructed(LoadedWorld& world, const entity::Aabb& box, std::span<const entity::Aabb> players);

} // namespace mobSpawnChecks

} // namespace basalt::server::runtime::world
