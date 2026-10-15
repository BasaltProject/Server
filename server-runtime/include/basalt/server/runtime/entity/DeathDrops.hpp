#pragma once

#include <cstdint>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `LivingEntity.dropAllDeathLoot` and the overrides that decide it per class.
namespace deathDrops {

/// parity: `LivingEntity.shouldDropLoot`, and `Monster`'s.
[[nodiscard]] bool dropsLoot(const ServerEntity& entity, bool mobDrops);

/// parity: `LivingEntity.shouldDropExperience` and its overrides.
[[nodiscard]] bool shouldDropExperience(const ServerEntity& entity);

/// parity: `LivingEntity.dropExperience`.
[[nodiscard]] bool dropsExperience(const ServerEntity& entity, bool mobDrops);

/// parity: `getBaseExperienceReward` for each class.
[[nodiscard]] std::int32_t experienceReward(const ServerEntity& entity, random::RandomSource& random);

} // namespace deathDrops

} // namespace basalt::server::runtime::entity
