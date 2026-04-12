#pragma once

#include <cstdint>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// What a living thing leaves behind when it dies, besides its body.
///
/// parity: `LivingEntity.dropAllDeathLoot` and the overrides that decide it per class.
namespace deathDrops {

/// Whether its loot table rolls. parity: `LivingEntity.shouldDropLoot`, and `Monster`'s.
[[nodiscard]] bool dropsLoot(const ServerEntity& entity, bool mobDrops);

/// parity: `LivingEntity.shouldDropExperience` and its overrides, which a sculk catalyst asks alone.
[[nodiscard]] bool shouldDropExperience(const ServerEntity& entity);

/// Whether it leaves experience. parity: `LivingEntity.dropExperience`.
[[nodiscard]] bool dropsExperience(const ServerEntity& entity, bool mobDrops);

/// parity: `getBaseExperienceReward` for each class. Draws from `random` like the mob's own.
[[nodiscard]] std::int32_t experienceReward(const ServerEntity& entity, random::RandomSource& random);

} // namespace deathDrops

} // namespace basalt::server::runtime::entity
