#include <basalt/server/runtime/player/PlayerPersistence.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

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
    data.putBoolean("seenCredits", session.seenCredits);
    // parity: `Entity.saveWithoutId` and `LivingEntity.addAdditionalSaveData`.
    data.putShort("Air", static_cast<std::int16_t>(session.airSupply));
    data.putShort("Fire", static_cast<std::int16_t>(session.fireTicks));
    data.putInt("TicksFrozen", session.ticksFrozen);
    data.putDouble("fall_distance", session.fallDistance);
    data.putFloat("AbsorptionAmount", session.absorption);
    if (session.lastDeath.has_value()) {
        wire::nbt::NbtCompound death;
        death.putString("dimension", session.lastDeath->dimensionName);
        death.putIntArray(
            "pos", {session.lastDeath->location.x, session.lastDeath->location.y, session.lastDeath->location.z});
        data.set("LastDeathLocation", wire::nbt::NbtTag::ofCompound(std::move(death)));
    }
    if (session.shoulderLeft.has_value()) {
        data.set("ShoulderEntityLeft", wire::nbt::NbtTag::ofCompound(*session.shoulderLeft));
    }
    if (session.shoulderRight.has_value()) {
        data.set("ShoulderEntityRight", wire::nbt::NbtTag::ofCompound(*session.shoulderRight));
    }
    // parity: `LivingEntity.addAdditionalSaveData`, one `MobEffectInstance` each.
    if (!session.effects.empty()) {
        std::vector<wire::nbt::NbtTag> effects;
        for (const auto& [id, effect] : session.effects) {
            wire::nbt::NbtCompound entry;
            entry.putString("id", id.toString());
            entry.putByte("amplifier", static_cast<std::int8_t>(effect.amplifier));
            entry.putInt("duration", effect.remainingTicks);
            entry.putBoolean("ambient", effect.ambient);
            entry.putBoolean("show_particles", effect.visible);
            entry.putBoolean("show_icon", effect.showIcon);
            effects.push_back(wire::nbt::NbtTag::ofCompound(std::move(entry)));
        }
        data.set("active_effects",
                 wire::nbt::NbtTag::ofList(wire::nbt::NbtList{wire::nbt::NbtType::Compound, std::move(effects)}));
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
    session.seenCredits = stored.entityData.getBoolean("seenCredits");
    if (stored.entityData.contains("Air")) {
        session.airSupply = stored.entityData.getShort("Air");
    }
    session.fireTicks = std::max<std::int32_t>(0, stored.entityData.getShort("Fire"));
    session.ticksFrozen = std::max<std::int32_t>(0, stored.entityData.getInt("TicksFrozen"));
    session.fallDistance = stored.entityData.getDouble("fall_distance");
    session.absorption = stored.entityData.getFloat("AbsorptionAmount");
    session.lastDeath.reset();
    if (const wire::nbt::NbtCompound* death = stored.entityData.getCompound("LastDeathLocation"); death != nullptr) {
        const std::span<const std::int32_t> pos = death->getIntArray("pos");
        if (pos.size() == 3) {
            session.lastDeath = wire::protocol::play::GlobalPos{
                .dimensionName = std::string{death->getString("dimension")}, .location = {pos[0], pos[1], pos[2]}};
        }
    }
    session.shoulderLeft = shoulderOf(stored.entityData, "ShoulderEntityLeft");
    session.shoulderRight = shoulderOf(stored.entityData, "ShoulderEntityRight");
    session.effects.clear();
    if (const wire::nbt::NbtList* effects = stored.entityData.getList("active_effects"); effects != nullptr) {
        for (const wire::nbt::NbtTag& element : *effects) {
            const wire::nbt::NbtCompound* entry = element.get<wire::nbt::NbtCompound>();
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                entry == nullptr ? std::unexpected(wire::common::IdentifierError{})
                                 : wire::common::Identifier::parse(std::string{entry->getString("id")});
            if (!id.has_value() || entry->getInt("duration") == 0) {
                continue;
            }
            session.effects.insert_or_assign(*id,
                                             world::ActiveEffect{.effect = *id,
                                                                 .amplifier = entry->getByte("amplifier"),
                                                                 .remainingTicks = entry->getInt("duration"),
                                                                 .ambient = entry->getBoolean("ambient"),
                                                                 .visible = entry->getBoolean("show_particles", true),
                                                                 .showIcon = entry->getBoolean("show_icon", true)});
        }
    }
}

} // namespace basalt::server::runtime::player::playerPersistence
