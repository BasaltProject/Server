#pragma once

namespace basalt::server::runtime::config {
struct MobSettings;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// Applies the world's `overrides.<type>` table to a mob that is entering the world.
namespace mobOverrides {

void apply(ServerEntity& entity, const config::MobSettings& settings);

/// The world's mobGriefing rule, unless this mob's type overrides it.
[[nodiscard]] bool griefing(const ServerEntity& entity, bool worldRule);

} // namespace mobOverrides

} // namespace basalt::server::runtime::entity
