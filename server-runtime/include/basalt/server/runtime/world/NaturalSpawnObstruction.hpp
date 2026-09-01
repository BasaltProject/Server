#pragma once

#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// Whether a mob would fit where the spawner wants to put it.
///
/// parity: `SpawnPlacements` over `Level.noCollision`: the spawn box has to be clear of block
/// collision, the entity box has to be out of any liquid unless the mob lives in one, and nothing
/// already standing there may be in the way.
namespace naturalSpawnObstruction {

/// What the world says is at a block, or nothing when that block is not loaded.
using BlockAt = std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z)>;

/// The box a spawn is tested against, which is not the entity's own: vanilla asks for a slightly
/// different one so a mob is not refused for the block it is standing on.
[[nodiscard]] entity::Aabb spawnBox(const wire::common::Identifier& type, double x, double y, double z);

[[nodiscard]] entity::Aabb entityBox(const wire::common::Identifier& type, double x, double y, double z);

/// parity: the mobs that spawn in liquid, which the liquid check does not apply to.
[[nodiscard]] bool allowsLiquids(const wire::common::Identifier& type);

/// Whether the blocks around this position leave room for that type.
[[nodiscard]] bool blocksAllow(const wire::common::Identifier& type, double x, double y, double z,
                               const BlockAt& blockAt);

/// Whether one entity already there is in the way.
///
/// parity: two different questions with two different boxes: what can be collided with is refused
/// against the spawn box, and what blocks building is refused against the entity's own.
[[nodiscard]] bool entityBlocksSpawn(const entity::ServerEntity& entity, const entity::Aabb& spawnBox,
                                     const entity::Aabb& entityBox);

/// parity: the three vanilla entity families whose base collision shape takes part in
/// `noCollision(null, box)`.
[[nodiscard]] bool canBeCollidedWith(const wire::common::Identifier& type);

/// parity: living entities and the non-living constructors that set `Entity.blocksBuilding`.
[[nodiscard]] bool blocksBuilding(const wire::common::Identifier& type);

} // namespace naturalSpawnObstruction

} // namespace basalt::server::runtime::world
