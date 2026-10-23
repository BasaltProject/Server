#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

using SessionList = std::function<std::vector<player::PlayerSession*>()>;

/// A player seen as an entity, so the entity services answer for it the way they do for a mob.
[[nodiscard]] std::shared_ptr<server::plugins::PluginEntityRef>
playerEntityRef(world::LoadedWorld& world, std::int32_t entityId, SessionList sessions);

} // namespace basalt::server::runtime::plugins
