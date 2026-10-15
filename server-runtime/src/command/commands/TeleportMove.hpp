#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::command::commands::teleportMove {

struct Place {
    world::LoadedWorld* world = nullptr;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;
};

/// The entity running the command. parity: `CommandSourceStack.getEntityOrException`.
[[nodiscard]] Parsed<CommandTarget> self(const CommandContext& context);

/// parity: `TeleportCommand.performTeleport` for a player, an entity or an item.
void move(const CommandContext& context, const CommandTarget& target, const Place& to);

} // namespace basalt::server::runtime::command::commands::teleportMove
