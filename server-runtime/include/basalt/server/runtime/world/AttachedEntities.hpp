#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

/// The entities that hang off or sit on a block: item frames, paintings and 26.3's cushion.
///
/// parity: `BlockAttachedEntity.tick`: every hundred ticks it asks whether it still has something to
/// hold it, and one that does not comes off and drops as an item.
namespace attachedEntities {

/// parity: `BlockAttachedEntity.CHECK_INTERVAL`.
inline constexpr std::int32_t kCheckInterval = 100;

[[nodiscard]] bool isAttached(const wire::common::Identifier& type);

/// One tick of the counter. parity: `ticksSinceLastCheck++ >= 100`, so the check is on the tick
/// after the hundredth, and the count starts again.
[[nodiscard]] bool dueForCheck(entity::ServerEntity& attached);

/// parity: `tickAtCheckInterval` and `survives`: a cushion is also destroyed in fire or lava.
[[nodiscard]] bool survives(const WorldState& world, const entity::ServerEntity& attached,
                            const cushions::Tagged& tagged);

/// What comes off with it when it falls. parity: `dropItem(level, null)` for each of the three, with
/// the frame's own `dropChance` rolled for what it held.
[[nodiscard]] std::vector<server::world::loot::ItemStack> drops(const entity::ServerEntity& attached,
                                                                random::RandomSource& random);

/// The sound it makes coming off.
[[nodiscard]] std::string_view breakSound(const wire::common::Identifier& type);

/// Takes it off: gone, its sound to everyone near, and its drops when `entity_drops` allows them.
void comeOff(LoadedWorld& world, entity::ServerEntity& attached, const std::vector<player::PlayerSession*>& watchers,
             random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId);

} // namespace attachedEntities

} // namespace basalt::server::runtime::world
