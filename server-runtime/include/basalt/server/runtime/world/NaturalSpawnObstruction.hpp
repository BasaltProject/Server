#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `SpawnPlacements`.
namespace naturalSpawnObstruction {

using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// Not the entity's own box: vanilla asks a slightly different one so a mob is not refused for the block it stands on.
[[nodiscard]] entity::Aabb spawnBox(const wire::common::Identifier& type, double x, double y, double z);

[[nodiscard]] entity::Aabb entityBox(const wire::common::Identifier& type, double x, double y, double z);

/// parity: the mobs that spawn in liquid, which the liquid check does not apply to.
[[nodiscard]] bool allowsLiquids(const wire::common::Identifier& type);

[[nodiscard]] bool blocksAllow(const wire::common::Identifier& type, double x, double y, double z,
                               const BlockAt& blockAt);

/// parity: what can be collided with is tested against the spawn box, what blocks building against the entity's own.
[[nodiscard]] bool entityBlocksSpawn(const entity::ServerEntity& entity, const entity::Aabb& spawnBox,
                                     const entity::Aabb& entityBox);

/// parity: `noCollision(null, box)`.
[[nodiscard]] bool canBeCollidedWith(const wire::common::Identifier& type);

/// parity: living entities and the non-living constructors that set `Entity.blocksBuilding`.
[[nodiscard]] bool blocksBuilding(const wire::common::Identifier& type);

} // namespace naturalSpawnObstruction

} // namespace basalt::server::runtime::world
