#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/RaiderEquipment.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using entity::Raid;
using entity::RaidPos;
using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::BossEventPacket;

/// parity: `PathfindToRaidGoal.recruitNearby`'s sixteen blocks.
constexpr double kRecruitRange = 16.0;
constexpr std::int32_t kSettledTicks = 600;
constexpr std::int64_t kJoinInterval = 20;

[[nodiscard]] RaidPos blockOf(const ServerEntity& mob) {
    return {static_cast<std::int32_t>(std::floor(mob.x)), static_cast<std::int32_t>(std::floor(mob.y)),
            static_cast<std::int32_t>(std::floor(mob.z))};
}

[[nodiscard]] std::int64_t distanceSquared(const RaidPos& from, const RaidPos& to) {
    const std::int64_t dx = from[0] - to[0];
    const std::int64_t dy = from[1] - to[1];
    const std::int64_t dz = from[2] - to[2];
    return (dx * dx) + (dy * dy) + (dz * dz);
}

void sendBar(PlayerSession& player, const Raid& raid, BossEventPacket::Operation operation) {
    if (player.connection().showBossBar) {
        const bool removing = std::holds_alternative<BossEventPacket::Remove>(operation);
        player.connection().showBossBar(raid.barId, removing ? wire::text::Component::empty() : raid.barName,
                                        removing ? 0.0F : raid.barProgress, !removing);
        return;
    }
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            BossEventPacket{.id = raid.barId, .operation = std::move(operation)}.write(buffer);
        });
    }
}

[[nodiscard]] BossEventPacket::Add addOf(const Raid& raid) {
    return BossEventPacket::Add{.name = raid.barName,
                                .progress = raid.barProgress,
                                .color = static_cast<wire::protocol::play::BossBarColor>(raid.barColor),
                                .overlay = static_cast<wire::protocol::play::BossBarOverlay>(raid.barStyle),
                                .darkenScreen = (raid.barFlags & 1) != 0,
                                .playMusic = (raid.barFlags & 2) != 0,
                                .createWorldFog = (raid.barFlags & 4) != 0};
}

} // namespace

bool RaidSystem::isRaider(const ServerEntity& mob) {
    const std::string_view path = mob.type().path();
    return path == "pillager" || path == "vindicator" || path == "evoker" || path == "illusioner" || path == "witch" ||
           path == "ravager";
}

void RaidSystem::setCelebrating(ServerEntity& raider, bool celebrating) { raider.celebrating = celebrating; }

bool RaidSystem::canJoinRaid(const ServerEntity& raider) {
    return !raider.removed && raider.health > 0.0 && raider.canJoinRaid && raider.idleTicks <= Raid::kMaxNoActionTime;
}

void RaidSystem::joinRaid(world::LoadedWorld& world, Raid& raid, std::int32_t wave, ServerEntity& raider) {
    // parity: `addWaveMob` adds the health even for a raider it already had.
    raid.addWaveMob(wave, raider.entityId());
    raid.totalHealth += static_cast<float>(raider.health);
    updateBossbar(world, raid);
    world.raids().dirty = true;
    raider.raidId = world.raids().idOf(&raid);
    raider.raidStale = false;
    raider.raidWave = wave;
    raider.canJoinRaid = true;
    raider.ticksOutsideRaid = 0;
}

void RaidSystem::removeFromRaid(world::LoadedWorld& world, Raid& raid, ServerEntity& raider, bool fromTotalHealth) {
    if (!raid.removeRaider(raider.raidWave, raider.entityId())) {
        return;
    }
    if (fromTotalHealth) {
        raid.totalHealth -= static_cast<float>(raider.health);
    }
    raider.raidId.reset();
    raider.raidStale = false;
    updateBossbar(world, raid);
    world.raids().dirty = true;
}

void RaidSystem::releaseRaiders(world::LoadedWorld& world, Raid& raid) {
    for (const auto& [wave, raiders] : raid.groups) {
        for (const std::int32_t id : raiders) {
            if (ServerEntity* raider = world.state().entity(id); raider != nullptr) {
                raider->raidStale = true;
            }
        }
    }
}

void RaidSystem::setLeader(world::LoadedWorld& world, Raid& raid, std::int32_t wave, ServerEntity& raider) {
    raid.leaders.insert_or_assign(wave, raider.entityId());
    entity::raiderEquipment::wearOminousBanner(raider);
    sendEquipment(world, raider);
}

void RaidSystem::updateRaiders(world::LoadedWorld& world, Raid& raid) {
    std::vector<std::pair<std::int32_t, std::int32_t>> leaving;
    for (const auto& [wave, raiders] : raid.groups) {
        for (const std::int32_t id : raiders) {
            const ServerEntity* raider = world.state().entity(id);
            if (raider == nullptr || raider->removed ||
                distanceSquared(raid.center, blockOf(*raider)) >= Raid::kRemovalRadiusSquared) {
                leaving.emplace_back(wave, id);
                continue;
            }
            if (raider->tickCount <= kSettledTicks) {
                continue;
            }
            ServerEntity& settled = *world.state().entity(id);
            if (!isVillage(world, blockOf(settled)) && settled.idleTicks > Raid::kMaxNoActionTime) {
                settled.ticksOutsideRaid++;
            }
            if (settled.ticksOutsideRaid >= Raid::kOutsideRaidTimeout) {
                leaving.emplace_back(wave, id);
            }
        }
    }
    for (const auto& [wave, id] : leaving) {
        ServerEntity* raider = world.state().entity(id);
        if (raider == nullptr) {
            raid.removeRaider(wave, id);
            updateBossbar(world, raid);
            continue;
        }
        removeFromRaid(world, raid, *raider, true);
        if (raider->patrolLeader) {
            raid.leaders.erase(raider->raidWave);
        }
    }
}

namespace {

[[nodiscard]] float healthOf(world::LoadedWorld& world, const Raid& raid, std::optional<std::int32_t> hurtId,
                             double hurtHealth) {
    float health = 0.0F;
    for (const auto& [wave, raiders] : raid.groups) {
        for (const std::int32_t id : raiders) {
            if (id == hurtId) {
                health += static_cast<float>(hurtHealth);
            } else if (const ServerEntity* raider = world.state().entity(id); raider != nullptr) {
                health += static_cast<float>(raider->health);
            }
        }
    }
    return health;
}

} // namespace

void RaidSystem::updateBossbar(world::LoadedWorld& world, Raid& raid) {
    const float health = healthOf(world, raid, std::nullopt, 0.0);
    raid.barStamp = entity::entityCombat::nextRaidBarStamp();
    // parity: `Mth.clamp(health / totalHealth, 0, 1)`; nothing spawned yet reads as empty.
    setBarProgress(world, raid, raid.totalHealth > 0.0F ? std::clamp(health / raid.totalHealth, 0.0F, 1.0F) : 0.0F);
}

void RaidSystem::applyRaiderHurts(world::LoadedWorld& world, Raid& raid) {
    const ServerEntity* latest = nullptr;
    for (const auto& [wave, raiders] : raid.groups) {
        for (const std::int32_t id : raiders) {
            const ServerEntity* raider = world.state().entity(id);
            if (raider != nullptr && raider->raidHurtStamp > raid.barStamp &&
                (latest == nullptr || raider->raidHurtStamp > latest->raidHurtStamp)) {
                latest = raider;
            }
        }
    }
    if (latest == nullptr) {
        return;
    }
    const float health = healthOf(world, raid, latest->entityId(), latest->raidHealthAtHurt);
    raid.barStamp = latest->raidHurtStamp;
    setBarProgress(world, raid, raid.totalHealth > 0.0F ? std::clamp(health / raid.totalHealth, 0.0F, 1.0F) : 0.0F);
}

void RaidSystem::updatePlayers(world::LoadedWorld& world, Raid& raid) {
    const std::vector<PlayerSession*> players = playersIn(world);
    std::set<std::int32_t> inRaid;
    for (PlayerSession* player : players) {
        if (!player->dead && raidAt(world, player->x, player->y, player->z) == &raid) {
            inRaid.insert(player->entityId());
            if (raid.barPlayers.insert(player->entityId()).second && raid.barVisible) {
                sendBar(*player, raid, addOf(raid));
            }
        }
    }
    for (PlayerSession* player : players) {
        if (!inRaid.contains(player->entityId()) && raid.barPlayers.erase(player->entityId()) > 0 && raid.barVisible) {
            sendBar(*player, raid, BossEventPacket::Remove{});
        }
    }
    std::erase_if(raid.barPlayers, [&inRaid](std::int32_t id) { return !inRaid.contains(id); });
}

void RaidSystem::setBarName(world::LoadedWorld& world, Raid& raid, wire::text::Component name) {
    if (name == raid.barName) {
        return;
    }
    raid.barName = std::move(name);
    if (!raid.barVisible) {
        return;
    }
    for (PlayerSession* player : playersIn(world)) {
        if (raid.barPlayers.contains(player->entityId())) {
            sendBar(*player, raid, BossEventPacket::UpdateName{.name = raid.barName});
        }
    }
}

void RaidSystem::setBarProgress(world::LoadedWorld& world, Raid& raid, float progress) {
    if (progress == raid.barProgress) {
        return;
    }
    raid.barProgress = progress;
    if (!raid.barVisible) {
        return;
    }
    for (PlayerSession* player : playersIn(world)) {
        if (raid.barPlayers.contains(player->entityId())) {
            sendBar(*player, raid, BossEventPacket::UpdateProgress{.progress = progress});
        }
    }
}

void RaidSystem::setBarVisible(world::LoadedWorld& world, Raid& raid, bool visible) {
    if (visible == raid.barVisible) {
        return;
    }
    raid.barVisible = visible;
    for (PlayerSession* player : playersIn(world)) {
        if (raid.barPlayers.contains(player->entityId())) {
            sendBar(*player, raid, visible ? BossEventPacket::Operation{addOf(raid)} : BossEventPacket::Remove{});
        }
    }
}

bool RaidSystem::controlBar(world::LoadedWorld& world, const std::int32_t id, const std::string_view action,
                            const std::string_view text, const std::int32_t value) {
    Raid* raid = raidsIn(world).get(id);
    if (raid == nullptr) {
        return false;
    }
    const auto toWatchers = [&](const BossEventPacket::Operation& operation) {
        if (!raid->barVisible) {
            return;
        }
        for (PlayerSession* player : playersIn(world)) {
            if (raid->barPlayers.contains(player->entityId())) {
                sendBar(*player, *raid, operation);
            }
        }
    };
    if (action == "bar_title") {
        const auto parsed = wire::common::parseJson(text);
        setBarName(world, *raid,
                   parsed.has_value() ? wire::text::componentJson::deserialize(*parsed)
                                      : wire::text::Component::text(std::string{text}));
    } else if (action == "bar_color" || action == "bar_style") {
        (action == "bar_color" ? raid->barColor : raid->barStyle) = std::clamp(value, 0, action == "bar_color" ? 6 : 4);
        toWatchers(
            BossEventPacket::UpdateStyle{.color = static_cast<wire::protocol::play::BossBarColor>(raid->barColor),
                                         .overlay = static_cast<wire::protocol::play::BossBarOverlay>(raid->barStyle)});
    } else if (action == "bar_flags") {
        raid->barFlags = value & 7;
        toWatchers(BossEventPacket::UpdateProperties{.darkenScreen = (raid->barFlags & 1) != 0,
                                                     .playMusic = (raid->barFlags & 2) != 0,
                                                     .createWorldFog = (raid->barFlags & 4) != 0});
    } else if (action == "bar_progress") {
        setBarProgress(world, *raid, std::clamp(static_cast<float>(value) / 10000.0F, 0.0F, 1.0F));
    } else if (action == "bar_visible") {
        setBarVisible(world, *raid, value != 0);
    } else if (action == "bar_add" || action == "bar_remove") {
        for (PlayerSession* player : playersIn(world)) {
            if (player->entityId() != value) {
                continue;
            }
            const bool adding = action == "bar_add";
            if (adding && raid->barPlayers.insert(value).second && raid->barVisible) {
                sendBar(*player, *raid, addOf(*raid));
            } else if (!adding && raid->barPlayers.erase(value) > 0 && raid->barVisible) {
                sendBar(*player, *raid, BossEventPacket::Remove{});
            }
            return true;
        }
        return false;
    } else if (action == "bar_clear") {
        removeAllBarPlayers(world, *raid);
    } else {
        return false;
    }
    return true;
}

void RaidSystem::removeAllBarPlayers(world::LoadedWorld& world, Raid& raid) {
    for (PlayerSession* player : playersIn(world)) {
        if (raid.barPlayers.contains(player->entityId()) && raid.barVisible) {
            sendBar(*player, raid, BossEventPacket::Remove{});
        }
    }
    raid.barPlayers.clear();
}

void RaidSystem::sendEquipment(world::LoadedWorld& world, const ServerEntity& raider) {
    using wire::protocol::play::SetEquipmentPacket;
    if (!raider.sourceNbt.has_value()) {
        return;
    }
    std::vector<SetEquipmentPacket::Slot> slots;
    for (const entity::GeneratedEquipment& slot : entity::generatedEntityEquipment(*raider.sourceNbt)) {
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(slot.item);
        const server::registry::Item* item = id.has_value() ? server::registry::items::registry().byName(*id) : nullptr;
        if (item == nullptr) {
            continue;
        }
        slots.push_back(SetEquipmentPacket::Slot{
            .type = slot.slot,
            .stack = wire::protocol::play::WireItemStack{
                .itemId = item->protocolId,
                .count = slot.count,
                .components = item::itemComponentCodecs::toWire(
                    slot.components, [](const wire::common::Identifier&) { return std::optional<std::int32_t>{}; })}});
    }
    const SetEquipmentPacket packet{.entityId = raider.entityId(), .slots = std::move(slots)};
    for (PlayerSession* player : playersIn(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEquipmentPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

void RaidSystem::tickRaider(world::LoadedWorld& world, ServerEntity& raider) {
    if (raider.removed || raider.health <= 0.0) {
        return;
    }
    // parity: `Raider.updateNoActionTime`, two a tick rather than one.
    raider.idleTicks++;
    Raid* raid = raider.raidId.has_value() ? world.raids().get(*raider.raidId) : nullptr;
    // parity: `readAdditionalSaveData` finds no raid under a saved id that is gone.
    if (raider.raidId.has_value() && raid == nullptr && !raider.raidStale) {
        raider.raidId.reset();
    }
    // parity: `readAdditionalSaveData`'s `addWaveMob` without health.
    if (raid != nullptr && !raid->has(raider.entityId())) {
        raid->addWaveMob(raider.raidWave, raider.entityId());
        if (raider.patrolLeader) {
            raid->leaders.insert_or_assign(raider.raidWave, raider.entityId());
        }
    }
    if (!raider.canJoinRaid) {
        return;
    }
    if (!raider.raidId.has_value()) {
        if (world.gameTime() % kJoinInterval == 0) {
            if (Raid* nearby = raidAt(world, raider.x, raider.y, raider.z); nearby != nullptr && canJoinRaid(raider)) {
                joinRaid(world, *nearby, nearby->groupsSpawned, raider);
            }
        }
        return;
    }
    if (raider.targetId.has_value()) {
        const ServerEntity* target = world.state().entity(*raider.targetId);
        if (target == nullptr || target->type().path() == "iron_golem") {
            raider.idleTicks = 0;
        }
    }
}

std::optional<entity::ai::MobContext::RaiderRaid> RaidSystem::raidOf(world::LoadedWorld& world,
                                                                     const ServerEntity& raider) {
    if (!raider.raidId.has_value()) {
        return std::nullopt;
    }
    const Raid* raid = world.raids().get(*raider.raidId);
    if (raid == nullptr) {
        // parity: a raid stopped under a raider stays its raid, inactive, until it is saved.
        return entity::ai::MobContext::RaiderRaid{};
    }
    bool leaderAlive = false;
    if (const auto leader = raid->leaders.find(raider.raidWave); leader != raid->leaders.end()) {
        const ServerEntity* found = world.state().entity(leader->second);
        leaderAlive = found != nullptr && !found->removed && found->health > 0.0;
    }
    return entity::ai::MobContext::RaiderRaid{.active = raid->active,
                                              .over = raid->isOver(),
                                              .loss = raid->status == entity::RaidStatus::Loss,
                                              .center = raid->center,
                                              .leaderAlive = leaderAlive};
}

void RaidSystem::raiderDied(world::LoadedWorld& world, ServerEntity& raider,
                            const std::optional<wire::common::Uuid>& playerKiller) {
    Raid* raid = raider.raidId.has_value() ? world.raids().get(*raider.raidId) : nullptr;
    if (raid == nullptr) {
        return;
    }
    if (raider.patrolLeader) {
        raid->leaders.erase(raider.raidWave);
    }
    if (playerKiller.has_value() && !std::ranges::contains(raid->heroes, *playerKiller)) {
        raid->heroes.push_back(*playerKiller);
    }
    removeFromRaid(world, *raid, raider, false);
}

void RaidSystem::recruitAround(world::LoadedWorld& world, ServerEntity& raider) {
    Raid* raid = raider.raidId.has_value() ? world.raids().get(*raider.raidId) : nullptr;
    if (raid == nullptr || !raid->active) {
        return;
    }
    const entity::Aabb box =
        entity::Aabb::of(raider.x, raider.y, raider.z, raider.width(), raider.height()).inflate(kRecruitRange);
    for (ServerEntity* other : world.state().entities()) {
        if (other == nullptr || other == &raider || !isRaider(*other) || !canJoinRaid(*other)) {
            continue;
        }
        const Raid* its = other->raidId.has_value() ? world.raids().get(*other->raidId) : nullptr;
        if ((its != nullptr && its->active) ||
            !box.intersects(entity::Aabb::of(other->x, other->y, other->z, other->width(), other->height()))) {
            continue;
        }
        joinRaid(world, *raid, raid->groupsSpawned, *other);
    }
}

void RaidSystem::takeBanner(world::LoadedWorld& world, ServerEntity& raider, std::int32_t itemId) {
    Raid* raid = raider.raidId.has_value() ? world.raids().get(*raider.raidId) : nullptr;
    if (raid == nullptr || !raid->active || raid->leaders.contains(raider.raidWave)) {
        return;
    }
    const std::optional<world::DroppedItem> banner = world.state().removeItem(itemId);
    if (!banner.has_value()) {
        return;
    }
    if (!entity::raiderEquipment::isOminousBanner(banner->item, banner->count, banner->components)) {
        static_cast<void>(world.state().restoreItem(*banner));
        return;
    }
    const wire::protocol::play::TakeItemEntityPacket packet{
        .collectedEntityId = itemId, .collectorEntityId = raider.entityId(), .pickupItemCount = banner->count};
    for (PlayerSession* player : playersIn(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::TakeItemEntityPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
    raider.patrolLeader = true;
    setLeader(world, *raid, raider.raidWave, raider);
}

} // namespace basalt::server::runtime
