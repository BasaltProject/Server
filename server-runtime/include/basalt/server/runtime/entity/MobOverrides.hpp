#pragma once

#include <vector>

namespace basalt::server::runtime::config {
struct MobSettings;
}

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::world::loot {
struct ItemStack;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Applies the world's `overrides.<type>` table to a mob that is entering the world.
namespace mobOverrides {

void apply(ServerEntity& entity, const config::MobSettings& settings);

/// The world's mobGriefing rule, unless this mob's type overrides it.
[[nodiscard]] bool griefing(const ServerEntity& entity, bool worldRule);

/// Rolls this type's configured drops into the loot, replacing it first when the override says so.
void customDrops(const ServerEntity& dead, std::vector<server::world::loot::ItemStack>& loot,
                 random::RandomSource& random);

} // namespace mobOverrides

} // namespace basalt::server::runtime::entity
