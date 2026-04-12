#include <basalt/server/runtime/player/PlayerPersistence.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime::player::playerPersistence {
namespace {

[[nodiscard]] storage::StoredGamemode storedGamemodeOf(wire::protocol::play::Gamemode gamemode) {
    return static_cast<storage::StoredGamemode>(static_cast<std::uint8_t>(gamemode));
}

[[nodiscard]] wire::protocol::play::Gamemode gamemodeOf(storage::StoredGamemode gamemode) {
    return static_cast<wire::protocol::play::Gamemode>(static_cast<std::uint8_t>(gamemode));
}

/// parity: `Player.addAdditionalSaveData`'s `Score` and `ServerPlayer`'s shoulder parrots.
[[nodiscard]] wire::nbt::NbtCompound entityDataOf(const PlayerSession& session) {
    wire::nbt::NbtCompound data;
    data.putInt("Score", session.experience.score());
    if (session.shoulderLeft.has_value()) {
        data.set("ShoulderEntityLeft", wire::nbt::NbtTag::ofCompound(*session.shoulderLeft));
    }
    if (session.shoulderRight.has_value()) {
        data.set("ShoulderEntityRight", wire::nbt::NbtTag::ofCompound(*session.shoulderRight));
    }
    return data;
}

[[nodiscard]] std::optional<wire::nbt::NbtCompound> shoulderOf(const wire::nbt::NbtCompound& data,
                                                               std::string_view key) {
    const wire::nbt::NbtCompound* shoulder = data.getCompound(key);
    return shoulder == nullptr || shoulder->empty() ? std::nullopt : std::optional{*shoulder};
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
                                 .tags = session.tags,
                                 .postEffects = session.postEffects,
                                 .wardenSpawnTracker = {session.wardenTracker.ticksSinceLastWarning,
                                                        session.wardenTracker.warningLevel,
                                                        session.wardenTracker.cooldownTicks},
                                 .entityData = entityDataOf(session)};
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
    session.postEffects = stored.postEffects;
    session.wardenTracker = world::wardenSpawnTracker::Tracker{.ticksSinceLastWarning = stored.wardenSpawnTracker[0],
                                                               .warningLevel = stored.wardenSpawnTracker[1],
                                                               .cooldownTicks = stored.wardenSpawnTracker[2]};
    session.experience.setScore(stored.entityData.getInt("Score"));
    session.shoulderLeft = shoulderOf(stored.entityData, "ShoulderEntityLeft");
    session.shoulderRight = shoulderOf(stored.entityData, "ShoulderEntityRight");
}

} // namespace basalt::server::runtime::player::playerPersistence
