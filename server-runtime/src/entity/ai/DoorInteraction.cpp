#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>

namespace basalt::server::runtime::entity::ai::doorInteraction {
namespace {

/// parity: `MOB_INTERACTABLE_DOORS`, and whether that door stands open.
[[nodiscard]] std::optional<bool> doorOpen(world::WorldState& world, const Block& pos) {
    const std::optional<std::int32_t> state = world.blockState(pos[0], pos[1], pos[2]);
    if (!state.has_value() || !pathFinder::isMobDoor(*state)) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = server::registry::blockStateDefinitions::propertiesOf(*state);
    const auto open = properties.find("open");
    return open != properties.end() && open->second == "true";
}

/// parity: `BlockPos.closerToCenterThan`.
[[nodiscard]] bool closerToCenter(const Block& pos, const ServerEntity& mob, double distance) {
    const double dx = pos[0] + 0.5 - mob.x;
    const double dy = pos[1] + 0.5 - mob.y;
    const double dz = pos[2] + 0.5 - mob.z;
    return (dx * dx) + (dy * dy) + (dz * dz) < distance * distance;
}

/// parity: `isMobComingThroughDoor`, the door is one end of the step the other mob is on.
[[nodiscard]] bool comingThrough(const ServerEntity& other, const Block& door) {
    if (!other.path.has_value() || other.pathIndex <= 0 ||
        other.pathIndex >= static_cast<std::int32_t>(other.path->size())) {
        return false;
    }
    return (*other.path)[static_cast<std::size_t>(other.pathIndex - 1)] == door ||
           (*other.path)[static_cast<std::size_t>(other.pathIndex)] == door;
}

} // namespace

void tick(world::WorldState& world, ServerEntity& mob, State& state, const std::vector<ServerEntity*>& nearest,
          const SetDoor& setDoor) {
    // parity: a path neither unstarted nor done.
    if (!mob.path.has_value() || mob.pathIndex <= 0 || mob.pathIndex >= static_cast<std::int32_t>(mob.path->size())) {
        return;
    }
    const Block to = (*mob.path)[static_cast<std::size_t>(mob.pathIndex)];
    if (state.lastCheckedNode == to) {
        state.remainingCooldown = 20;
    } else if (--state.remainingCooldown > 0) {
        return;
    }
    state.lastCheckedNode = to;
    const Block from = (*mob.path)[static_cast<std::size_t>(mob.pathIndex - 1)];
    const auto remember = [&state](const Block& door) {
        if (!std::ranges::contains(state.doorsToClose, door)) {
            state.doorsToClose.push_back(door);
        }
    };
    if (const std::optional<bool> open = doorOpen(world, from); open.has_value()) {
        if (!*open) {
            setDoor(from, true);
        }
        remember(from);
    }
    if (const std::optional<bool> open = doorOpen(world, to); open.has_value() && !*open) {
        setDoor(to, true);
        remember(to);
    }
    // parity: `closeDoorsThatIHaveOpenedOrPassedThrough`, which keeps the two it is stepping between.
    for (auto door = state.doorsToClose.begin(); door != state.doorsToClose.end();) {
        if (*door == from || *door == to) {
            ++door;
            continue;
        }
        const std::optional<bool> open = doorOpen(world, *door);
        if (closerToCenter(*door, mob, 3.0) && open == std::optional{true}) {
            const bool others = std::ranges::any_of(nearest, [&](const ServerEntity* other) {
                return other != &mob && other->type() == mob.type() && closerToCenter(*door, *other, 2.0) &&
                       comingThrough(*other, *door);
            });
            if (!others) {
                setDoor(*door, false);
            }
        }
        door = state.doorsToClose.erase(door);
    }
}

} // namespace basalt::server::runtime::entity::ai::doorInteraction
