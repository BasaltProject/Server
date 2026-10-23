#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::plugins {

/// A dropped item seen as an entity, so the entity services answer for it the way they do for a mob.
[[nodiscard]] std::shared_ptr<server::plugins::PluginEntityRef> droppedItemRef(world::LoadedWorld& world,
                                                                               std::int32_t entityId);

[[nodiscard]] std::vector<std::shared_ptr<server::plugins::PluginEntityRef>> droppedItemRefs(world::LoadedWorld& world);

/// Puts a stack on the ground: `stack` is the stack NBT, `entityId` is a fresh id. Answers the id or -1.
std::int32_t dropItem(world::LoadedWorld& world, std::int32_t entityId, std::span<const std::uint8_t> stack, double x,
                      double y, double z, double velocityX, double velocityY, double velocityZ);

} // namespace basalt::server::runtime::plugins
