#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::Raid;
using entity::RaidStatus;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::BossEventPacket;
using wire::protocol::play::Gamemode;
using wire::text::Component;

constexpr std::string_view kSoundRaidHorn = "event.raid.horn";

/// parity: the horn is heard sixty-four blocks away, at sixty-four blocks of volume.
constexpr double kRaidHornRangeSquared = 4096.0;
constexpr float kRaidHornVolume = 64.0F;
constexpr std::string_view kRaidBarKey = "event.minecraft.raid";

/// parity: `NUM_SPAWN_ATTEMPTS` and the ring `findRandomSpawnPos` looks along.
constexpr std::int32_t kRaidSpawnAttempts = 20;
constexpr double kRaidSpawnMin = 24.0;
constexpr double kRaidSpawnMax = 32.0;
constexpr std::int32_t kRaidSpawnLift = 12;
constexpr std::int32_t kRaidSpawnDrop = 24;

/// parity: `BadOmenMobEffect`, whose raid omen lasts thirty seconds.
constexpr std::int32_t kRaidOmenTicks = 600;

/// How often a bad omen looks for a village.
constexpr std::int32_t kVillageCheckInterval = 20;

void sendRaidBar(PlayerSession& player, const Raid& raid, BossEventPacket::Operation operation) {
    // A client that draws bars its own way is told what the bar says rather than handed Java's
    // packet: the shape of the operation is this edition's, the bar is not.
    if (player.connection().showBossBar) {
        if (const auto* add = std::get_if<BossEventPacket::Add>(&operation); add != nullptr) {
            player.connection().showBossBar(raid.barId(), add->name, add->progress, true);
            return;
        }
        if (const auto* progress = std::get_if<BossEventPacket::UpdateProgress>(&operation); progress != nullptr) {
            player.connection().showBossBar(raid.barId(), wire::text::Component::empty(), progress->progress, true);
            return;
        }
        player.connection().showBossBar(raid.barId(), wire::text::Component::empty(), 0.0F, false);
        return;
    }
    if (!player.connection().sendPlayPacket) {
        return;
    }
    player.connection().sendPlayPacket(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        BossEventPacket{.id = raid.barId(), .operation = std::move(operation)}.write(buffer);
    });
}

/// The first solid block under a column, which is where anything spawned outdoors stands.
std::optional<std::int32_t> groundBelow(world::LoadedWorld& world, std::int32_t x, std::int32_t fromY, std::int32_t z) {
    std::int32_t y = fromY;
    for (std::int32_t dropped = 0; dropped < kRaidSpawnDrop; dropped++) {
        const std::optional<std::int32_t> below = world.state().blockState(x, y - 1, z);
        const std::optional<std::int32_t> here = world.state().blockState(x, y, z);
        if (!below.has_value() || !here.has_value()) {
            return std::nullopt;
        }
        if (!server::registry::blockShapes::isEmptyShape(*below) &&
            server::registry::blockShapes::isEmptyShape(*here)) {
            return y;
        }
        y--;
    }
    return std::nullopt;
}

} // namespace

RaidSystem::RaidSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                       NaturalSpawningSystem& spawning, Hooks hooks, Options options, wire::common::Logger logger)
    : players_(&players), broadcaster_(&broadcaster), spawning_(&spawning), hooks_(std::move(hooks)),
      options_(std::move(options)), logger_(std::move(logger)) {}

entity::RaidManager& RaidSystem::raidsIn(const world::LoadedWorld& world) {
    const auto found = managers_.find(world.name());
    if (found != managers_.end()) {
        return *found->second;
    }
    return *managers_.emplace(world.name(), std::make_unique<entity::RaidManager>()).first->second;
}

std::optional<entity::villager::RaidView> RaidSystem::viewAt(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                             std::int32_t z) {
    const Raid* raid = raidsIn(world).nearby(x, y, z);
    if (raid == nullptr) {
        return std::nullopt;
    }
    return entity::villager::RaidView{.active = raid->active(),
                                      .victory = raid->status == RaidStatus::Victory,
                                      .loss = raid->status == RaidStatus::Loss,
                                      .stopped = raid->status == RaidStatus::Stopped,
                                      .firstWaveSpawned = raid->groupsSpawned > 0,
                                      .betweenWaves =
                                          raid->groupsSpawned > 0 && raid->raiders.empty() && raid->cooldown > 0};
}

void RaidSystem::tickRaids(world::LoadedWorld& world) {
    const auto found = managers_.find(world.name());
    if (found == managers_.end() || found->second->all().empty()) {
        return;
    }
    entity::RaidManager& manager = *found->second;
    std::vector<PlayerSession*> players;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            players.push_back(player);
        }
    }
    std::vector<Raid*> running;
    running.reserve(manager.all().size());
    for (const std::unique_ptr<Raid>& raid : manager.all()) {
        running.push_back(raid.get());
    }
    std::vector<Raid*> finished;
    for (Raid* raid : running) {
        tickRaid(world, players, *raid);
        if (raid->status == RaidStatus::Ongoing || raid->celebrationTicks > 0) {
            continue;
        }
        hideRaidBar(players, *raid);
        if (hooks_.raidFinished) {
            hooks_.raidFinished(world, *raid, raid->status == RaidStatus::Victory);
        }
        finished.push_back(raid);
    }
    manager.forget(finished);
}

void RaidSystem::tickRaid(world::LoadedWorld& world, const std::vector<PlayerSession*>& players, Raid& raid) {
    if (raid.status != RaidStatus::Ongoing) {
        raid.celebrationTicks--;
        return;
    }
    raid.ticksActive++;
    if (raid.ticksActive >= Raid::kTimeoutTicks) {
        raid.status = RaidStatus::Stopped;
        raid.celebrationTicks = 0;
        return;
    }
    double health = 0.0;
    std::erase_if(raid.raiders, [&world, &health](std::int32_t id) {
        const entity::ServerEntity* raider = world.state().entity(id);
        if (raider == nullptr || raider->removed || raider->health <= 0.0) {
            return true;
        }
        health += raider->health;
        return false;
    });
    // parity: `updatePlayers`, which is who the bar belongs to and who ends up a hero.
    for (const PlayerSession* player : players) {
        if (raid.distanceSquaredTo(player->x, player->y, player->z) < Raid::kValidRadiusSquared) {
            raid.heroes.insert(player->entityId());
        }
    }
    if (raid.raiders.empty()) {
        if (!raid.hasMoreWaves()) {
            winRaid(players, raid);
            return;
        }
        if (raid.cooldown > 0) {
            raid.cooldown--;
        } else {
            spawnRaidWave(world, players, raid);
            raid.cooldown = Raid::kPreRaidTicks;
        }
    }
    updateRaidBar(players, raid, health);
}

void RaidSystem::spawnRaidWave(world::LoadedWorld& world, const std::vector<PlayerSession*>& players, Raid& raid) {
    const std::int32_t wave = raid.groupsSpawned + 1;
    const std::optional<std::array<std::int32_t, 3>> position = raidSpawnPosition(world, raid);
    if (!position.has_value()) {
        logger_.debug([&] {
            return "Raid wave " + std::to_string(wave) + " found nowhere to spawn around " +
                   std::to_string(raid.centerX()) + ", " + std::to_string(raid.centerZ());
        });
        return;
    }
    double total = 0.0;
    for (const std::string& type : entity::raidWaves::types()) {
        const std::int32_t count = entity::raidWaves::countFor(type, wave);
        for (std::int32_t index = 0; index < count; index++) {
            entity::ServerEntity* raider =
                spawning_->spawnMob(world, Identifier::vanilla(type), (*position)[0] + 0.5,
                                    static_cast<double>((*position)[1]), (*position)[2] + 0.5);
            if (raider == nullptr) {
                continue;
            }
            raider->persistent = true;
            raider->raidCenter = std::array<std::int32_t, 3>{raid.centerX(), raid.centerY(), raid.centerZ()};
            raid.raiders.push_back(raider->entityId());
            total += raider->health;
        }
    }
    raid.groupsSpawned = wave;
    raid.totalHealth = total;
    // parity: `playSound`, the horn everyone within sixty-four blocks hears.
    std::vector<PlayerSession*> hearers;
    for (PlayerSession* player : players) {
        if (raid.distanceSquaredTo(player->x, player->y, player->z) < kRaidHornRangeSquared) {
            hearers.push_back(player);
        }
    }
    broadcaster_->sendEffects(hearers, (*position)[0], (*position)[1], (*position)[2],
                              server::registry::blocks::air().stateId,
                              {world::blockEffect::Sound{.name = std::string{kSoundRaidHorn},
                                                         .source = wire::protocol::play::SoundSource::Neutral,
                                                         .volume = kRaidHornVolume,
                                                         .pitch = 1.0F}});
}

std::optional<std::array<std::int32_t, 3>> RaidSystem::raidSpawnPosition(world::LoadedWorld& world,
                                                                         const Raid& raid) const {
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    for (std::int32_t attempt = 0; attempt < kRaidSpawnAttempts; attempt++) {
        const double angle = random.nextDouble() * 2.0 * std::numbers::pi;
        const double distance = kRaidSpawnMin + (random.nextDouble() * (kRaidSpawnMax - kRaidSpawnMin));
        const std::int32_t x = raid.centerX() + static_cast<std::int32_t>(std::cos(angle) * distance);
        const std::int32_t z = raid.centerZ() + static_cast<std::int32_t>(std::sin(angle) * distance);
        const std::optional<std::int32_t> ground = groundBelow(world, x, raid.centerY() + kRaidSpawnLift, z);
        if (ground.has_value()) {
            return std::array<std::int32_t, 3>{x, *ground, z};
        }
    }
    return std::nullopt;
}

void RaidSystem::winRaid(const std::vector<PlayerSession*>& players, Raid& raid) {
    raid.status = RaidStatus::Victory;
    raid.celebrationTicks = Raid::kCelebrationTicks;
    for (PlayerSession* player : players) {
        if (!raid.heroes.contains(player->entityId()) || !player->connection().addEffect) {
            continue;
        }
        player->connection().addEffect(world::mobEffects::heroOfTheVillage(), Raid::kHeroDurationTicks,
                                       raid.omenLevel - 1);
    }
}

void RaidSystem::updateRaidBar(const std::vector<PlayerSession*>& players, Raid& raid, double health) {
    if (raid.groupsSpawned == 0) {
        return;
    }
    const float progress =
        raid.totalHealth <= 0.0 ? 0.0F : std::clamp(static_cast<float>(health / raid.totalHealth), 0.0F, 1.0F);
    for (PlayerSession* player : players) {
        const bool inRaid = raid.distanceSquaredTo(player->x, player->y, player->z) < Raid::kValidRadiusSquared;
        if (!inRaid) {
            if (raid.watchers.erase(player->entityId()) > 0) {
                sendRaidBar(*player, raid, BossEventPacket::Remove{});
            }
            continue;
        }
        if (raid.watchers.insert(player->entityId()).second) {
            sendRaidBar(*player, raid,
                        BossEventPacket::Add{.name = Component::translatable(std::string{kRaidBarKey}),
                                             .progress = progress,
                                             .color = wire::protocol::play::BossBarColor::Red,
                                             .overlay = wire::protocol::play::BossBarOverlay::Notched10});
        } else {
            sendRaidBar(*player, raid, BossEventPacket::UpdateProgress{.progress = progress});
        }
    }
}

void RaidSystem::hideRaidBar(const std::vector<PlayerSession*>& players, Raid& raid) {
    for (PlayerSession* player : players) {
        if (raid.watchers.erase(player->entityId()) > 0) {
            sendRaidBar(*player, raid, BossEventPacket::Remove{});
        }
    }
}

void RaidSystem::tickOmens(world::LoadedWorld& world, PlayerSession& player, const world::ActiveEffect& active) {
    if (player.gamemode == Gamemode::Spectator) {
        return;
    }
    if (active.effect == world::mobEffects::badOmen()) {
        if (world.difficulty() == "peaceful") {
            return;
        }
        // The effect's own countdown is the clock here: it runs whether or not the day does.
        if (active.remainingTicks % kVillageCheckInterval != 0 || !isVillage(world, player.x, player.y, player.z)) {
            return;
        }
        const Raid* raid = raidsIn(world).nearby(player.x, player.y, player.z);
        if (raid != nullptr && raid->omenLevel >= Raid::kMaxOmenLevel) {
            return;
        }
        player.effects.erase(world::mobEffects::badOmen());
        if (player.connection().sendEffectRemoved) {
            player.connection().sendEffectRemoved(world::mobEffects::badOmen());
        }
        player.raidOmenPosition = std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(player.x)),
                                                              static_cast<std::int32_t>(std::floor(player.y)),
                                                              static_cast<std::int32_t>(std::floor(player.z))};
        if (player.connection().addEffect) {
            player.connection().addEffect(world::mobEffects::raidOmen(), kRaidOmenTicks, active.amplifier);
        }
        return;
    }
    if (active.effect != world::mobEffects::raidOmen() || active.remainingTicks > 1 ||
        !player.raidOmenPosition.has_value()) {
        return;
    }
    const std::array<std::int32_t, 3> position = *player.raidOmenPosition;
    player.raidOmenPosition.reset();
    const Raid* raid =
        raidsIn(world).createOrExtend(position[0], position[1], position[2], active.amplifier, world.difficulty());
    if (raid != nullptr) {
        logger_.debug([&] {
            return "Raid at " + std::to_string(raid->centerX()) + ", " + std::to_string(raid->centerY()) + ", " +
                   std::to_string(raid->centerZ()) + " in world '" + world.name() +
                   "': " + std::to_string(raid->totalGroups()) + " wave(s), omen level " +
                   std::to_string(raid->omenLevel);
        });
    }
}

bool RaidSystem::isVillage(world::LoadedWorld& world, double x, double y, double z) {
    return world.state().poi().isVillage(world::poi::BlockPos{static_cast<std::int32_t>(std::floor(x)),
                                                              static_cast<std::int32_t>(std::floor(y)),
                                                              static_cast<std::int32_t>(std::floor(z))});
}

} // namespace basalt::server::runtime
