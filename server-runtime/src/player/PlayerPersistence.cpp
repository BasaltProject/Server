#include <basalt/server/runtime/player/PlayerPersistence.hpp>

#include <string>

namespace basalt::server::runtime::player::playerPersistence {
namespace {

[[nodiscard]] storage::StoredGamemode storedGamemodeOf(wire::protocol::play::Gamemode gamemode) {
    return static_cast<storage::StoredGamemode>(static_cast<std::uint8_t>(gamemode));
}

[[nodiscard]] wire::protocol::play::Gamemode gamemodeOf(storage::StoredGamemode gamemode) {
    return static_cast<wire::protocol::play::Gamemode>(static_cast<std::uint8_t>(gamemode));
}

} // namespace

storage::StoredPlayer toStored(const PlayerSession& session, std::string_view world, std::int64_t firstSeenEpochSeconds,
                               std::int64_t playTimeTicks, std::int64_t nowEpochSeconds) {
    storage::StoredInventory enderChest;
    enderChest.slots.assign(session.enderChest.begin(), session.enderChest.end());
    return storage::StoredPlayer{.uuid = session.uuid().toString(),
                                 .name = session.name(),
                                 .lastSeenEpochSeconds = nowEpochSeconds,
                                 .firstSeenEpochSeconds = firstSeenEpochSeconds,
                                 .playTimeTicks = playTimeTicks,
                                 .world = std::string{world},
                                 .x = session.x,
                                 .y = session.y,
                                 .z = session.z,
                                 .yaw = session.yaw,
                                 .pitch = session.pitch,
                                 .health = session.health(),
                                 .food = session.food.foodLevel(),
                                 .saturation = session.food.saturationLevel(),
                                 .exhaustion = session.food.exhaustionLevel(),
                                 .xpLevel = session.experience.level(),
                                 .xpProgress = session.experience.progress(),
                                 .xpTotal = session.experience.total(),
                                 .gamemode = storedGamemodeOf(session.gamemode),
                                 .group = session.group,
                                 .inventory = session.inventory.snapshot(),
                                 .enderChest = std::move(enderChest),
                                 .stats = session.stats.snapshot(playTimeTicks),
                                 .tags = session.tags};
}

void restore(PlayerSession& session, const storage::StoredPlayer& stored) {
    session.x = stored.x;
    session.y = stored.y;
    session.z = stored.z;
    session.yaw = stored.yaw;
    session.pitch = stored.pitch;
    session.setHealth(stored.health);
    session.food.restore(stored.food, stored.saturation, stored.exhaustion);
    session.experience.restore(stored.xpLevel, stored.xpProgress, stored.xpTotal);
    session.gamemode = gamemodeOf(stored.gamemode);
    session.group = stored.group;
    session.flying = session.gamemode == wire::protocol::play::Gamemode::Spectator;
    session.stats.restore(stored.stats);
    session.inventory.restore(stored.inventory);
    for (std::size_t slot = 0; slot < session.enderChest.size(); slot++) {
        session.enderChest[slot] = slot < stored.enderChest.slots.size() ? stored.enderChest.slots[slot] : std::nullopt;
    }
    session.tags = stored.tags;
}

} // namespace basalt::server::runtime::player::playerPersistence
