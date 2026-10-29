#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::Raid;
using entity::RaidPos;
using entity::RaidStatus;
using player::PlayerSession;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `BadOmenMobEffect`'s raid omen, thirty seconds long.
constexpr std::int32_t kRaidOmenTicks = 600;
/// parity: `createOrExtendRaid`'s search for the village's points.
constexpr std::int32_t kVillageSearch = 64;
/// parity: `Raids.tick` saves every ten seconds even when nothing moved.
constexpr std::int32_t kSaveInterval = 200;
/// parity: `Raid.updatePlayers` and the name refresh run once a second.
constexpr std::int64_t kSecond = 20;
constexpr std::int32_t kLowMobThreshold = 2;
constexpr std::int32_t kSpawnAttempts = 5;
constexpr std::int32_t kCachedSpawnTries = 8;
constexpr std::int32_t kSpawnTries = 20;
constexpr std::int32_t kSectionRadius = 2;

[[nodiscard]] RaidPos blockOf(double x, double y, double z) {
    return {static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(y)),
            static_cast<std::int32_t>(std::floor(z))};
}

[[nodiscard]] Component raidName() { return Component::translatable("event.minecraft.raid"); }

} // namespace

RaidSystem::RaidSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                       NaturalSpawningSystem& spawning, Hooks hooks, Options options, wire::common::Logger logger)
    : players_(&players), broadcaster_(&broadcaster), spawning_(&spawning), hooks_(std::move(hooks)),
      options_(std::move(options)), logger_(std::move(logger)) {}

entity::RaidManager& RaidSystem::raidsIn(world::LoadedWorld& world) { return world.raids(); }

Raid* RaidSystem::raidAt(world::LoadedWorld& world, double x, double y, double z) {
    return world.raids().nearby(blockOf(x, y, z));
}

std::optional<entity::villager::RaidView> RaidSystem::viewAt(world::LoadedWorld& world, std::int32_t x, std::int32_t y,
                                                             std::int32_t z) {
    const Raid* raid = world.raids().nearby({x, y, z});
    if (raid == nullptr) {
        return std::nullopt;
    }
    return entity::villager::RaidView{.active = raid->active,
                                      .victory = raid->status == RaidStatus::Victory,
                                      .loss = raid->status == RaidStatus::Loss,
                                      .stopped = raid->isStopped(),
                                      .firstWaveSpawned = raid->hasFirstWaveSpawned(),
                                      .betweenWaves = raid->isBetweenWaves()};
}

std::vector<PlayerSession*> RaidSystem::playersIn(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> found;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            found.push_back(player);
        }
    }
    return found;
}

bool RaidSystem::isVillage(world::LoadedWorld& world, const RaidPos& pos) {
    return world.state().poi().isVillage(world::poi::BlockPos{pos[0], pos[1], pos[2]});
}

void RaidSystem::tickRaids(world::LoadedWorld& world) {
    entity::RaidManager& manager = world.raids();
    manager.tick++;
    const bool allowed = world.gameRules().get(world::gameRules::kRaids);
    manager.forEachInTickOrder([&](std::int32_t /*id*/, Raid& raid) {
        if (!allowed) {
            raid.stop();
            removeAllBarPlayers(world, raid);
        }
        if (raid.isStopped()) {
            removeAllBarPlayers(world, raid);
            releaseRaiders(world, raid);
            if (hooks_.raidFinished) {
                hooks_.raidFinished(world, raid, raid.endedAs == RaidStatus::Victory);
            }
            return true;
        }
        tickRaid(world, raid);
        return false;
    });
    if (manager.tick % kSaveInterval == 0) {
        manager.dirty = true;
    }
}

void RaidSystem::tickRaid(world::LoadedWorld& world, Raid& raid) {
    if (raid.isStopped()) {
        return;
    }
    if (raid.active) {
        applyRaiderHurts(world, raid);
    }
    if (raid.status == RaidStatus::Ongoing) {
        tickOngoing(world, raid);
    } else if (raid.isOver()) {
        tickCelebration(world, raid);
    }
}

void RaidSystem::tickOngoing(world::LoadedWorld& world, Raid& raid) {
    const bool wasActive = raid.active;
    raid.active = world.state().isLoaded(raid.center[0] >> 4, raid.center[2] >> 4);
    if (world.difficulty() == "peaceful") {
        raid.stop();
        removeAllBarPlayers(world, raid);
        return;
    }
    if (wasActive != raid.active) {
        setBarVisible(world, raid, raid.active);
    }
    if (!raid.active) {
        return;
    }
    if (!isVillage(world, raid.center)) {
        moveCenterToVillageSection(world, raid);
    }
    if (!isVillage(world, raid.center)) {
        if (raid.groupsSpawned > 0) {
            raid.status = RaidStatus::Loss;
        } else {
            raid.stop();
            removeAllBarPlayers(world, raid);
        }
    }
    if (++raid.ticksActive >= Raid::kTimeoutTicks) {
        raid.stop();
        removeAllBarPlayers(world, raid);
        return;
    }
    const std::int32_t raidersAlive = raid.totalRaidersAlive();
    if (raidersAlive == 0 && raid.hasMoreWaves()) {
        if (raid.cooldownTicks <= 0) {
            if (raid.cooldownTicks == 0 && raid.groupsSpawned > 0) {
                raid.cooldownTicks = Raid::kPreRaidTicks;
                setBarName(world, raid, raidName());
                return;
            }
        } else {
            const bool cached = raid.waveSpawnPos.has_value();
            bool shouldFind = !cached && raid.cooldownTicks % 5 == 0;
            if (cached && !world.state().isLoaded((*raid.waveSpawnPos)[0] >> 4, (*raid.waveSpawnPos)[2] >> 4)) {
                shouldFind = true;
            }
            if (shouldFind) {
                raid.waveSpawnPos = findRandomSpawnPos(world, raid, kCachedSpawnTries);
            }
            if (raid.cooldownTicks == Raid::kPreRaidTicks || raid.cooldownTicks % 20 == 0) {
                updatePlayers(world, raid);
            }
            --raid.cooldownTicks;
            setBarProgress(world, raid,
                           std::clamp(static_cast<float>(Raid::kPreRaidTicks - raid.cooldownTicks) /
                                          static_cast<float>(Raid::kPreRaidTicks),
                                      0.0F, 1.0F));
        }
    }
    if (raid.ticksActive % kSecond == 0) {
        updatePlayers(world, raid);
        updateRaiders(world, raid);
        if (raidersAlive > 0 && raidersAlive <= kLowMobThreshold) {
            setBarName(world, raid,
                       raidName()
                           .append(Component::text(" - "))
                           .append(Component::translatable("event.minecraft.raid.raiders_remaining",
                                                           {Component::text(std::to_string(raidersAlive))})));
        } else {
            setBarName(world, raid, raidName());
        }
    }
    bool hornPlayed = false;
    std::int32_t attempt = 0;
    while (raid.shouldSpawnGroup()) {
        const std::optional<RaidPos> spawnPos =
            raid.waveSpawnPos.has_value() ? raid.waveSpawnPos : findRandomSpawnPos(world, raid, kSpawnTries);
        if (spawnPos.has_value()) {
            raid.started = true;
            spawnGroup(world, raid, *spawnPos);
            if (!hornPlayed) {
                playHorn(world, raid, *spawnPos);
                hornPlayed = true;
            }
        } else {
            ++attempt;
        }
        if (attempt > kSpawnAttempts) {
            raid.stop();
            removeAllBarPlayers(world, raid);
            break;
        }
    }
    if (raid.started && !raid.hasMoreWaves() && raidersAlive == 0) {
        if (raid.postRaidTicks < Raid::kPostRaidTicks) {
            ++raid.postRaidTicks;
        } else {
            raid.status = RaidStatus::Victory;
            rewardHeroes(world, raid);
        }
    }
    world.raids().dirty = true;
}

void RaidSystem::tickCelebration(world::LoadedWorld& world, Raid& raid) {
    if (++raid.celebrationTicks >= Raid::kCelebrationTicks) {
        raid.stop();
        removeAllBarPlayers(world, raid);
        return;
    }
    if (raid.celebrationTicks % kSecond != 0) {
        return;
    }
    updatePlayers(world, raid);
    setBarVisible(world, raid, true);
    if (raid.status == RaidStatus::Victory) {
        setBarProgress(world, raid, 0.0F);
        setBarName(world, raid, Component::translatable("event.minecraft.raid.victory.full"));
    } else {
        setBarName(world, raid, Component::translatable("event.minecraft.raid.defeat.full"));
    }
}

void RaidSystem::moveCenterToVillageSection(world::LoadedWorld& world, Raid& raid) {
    // parity: `SectionPos.cube` walks x fastest, then y, then z; the first nearest wins.
    const std::int32_t sectionX = raid.center[0] >> 4;
    const std::int32_t sectionY = raid.center[1] >> 4;
    const std::int32_t sectionZ = raid.center[2] >> 4;
    std::optional<RaidPos> best;
    double bestDistance = 0.0;
    for (std::int32_t z = sectionZ - kSectionRadius; z <= sectionZ + kSectionRadius; ++z) {
        for (std::int32_t y = sectionY - kSectionRadius; y <= sectionY + kSectionRadius; ++y) {
            for (std::int32_t x = sectionX - kSectionRadius; x <= sectionX + kSectionRadius; ++x) {
                if (world.state().poi().sectionsToVillage(x, y, z) > 1) {
                    continue;
                }
                const RaidPos center{(x << 4) + 8, (y << 4) + 8, (z << 4) + 8};
                const auto dx = static_cast<double>(center[0] - raid.center[0]);
                const auto dy = static_cast<double>(center[1] - raid.center[1]);
                const auto dz = static_cast<double>(center[2] - raid.center[2]);
                const double distance = (dx * dx) + (dy * dy) + (dz * dz);
                if (!best.has_value() || distance < bestDistance) {
                    best = center;
                    bestDistance = distance;
                }
            }
        }
    }
    if (best.has_value()) {
        raid.center = *best;
    }
}

void RaidSystem::tickOmens(world::LoadedWorld& world, PlayerSession& player, const world::ActiveEffect& active) {
    if (active.effect == world::mobEffects::badOmen()) {
        const RaidPos pos = blockOf(player.x, player.y, player.z);
        if (player.gamemode == Gamemode::Spectator || world.difficulty() == "peaceful" || !isVillage(world, pos)) {
            return;
        }
        const Raid* raid = world.raids().nearby(pos);
        if (raid != nullptr && raid->raidOmenLevel >= Raid::kMaxOmenLevel) {
            return;
        }
        if (player.connection().addEffect) {
            player.connection().addEffect(world::mobEffects::raidOmen(), kRaidOmenTicks, active.amplifier);
        }
        player.raidOmenPosition = pos;
        // parity: `applyEffectTick` answering false takes the bad omen away.
        player.effects.erase(world::mobEffects::badOmen());
        if (player.connection().sendEffectRemoved) {
            player.connection().sendEffectRemoved(world::mobEffects::badOmen());
        }
        return;
    }
    if (active.effect != world::mobEffects::raidOmen() || active.remainingTicks != 1 ||
        player.gamemode == Gamemode::Spectator || !player.raidOmenPosition.has_value()) {
        return;
    }
    const RaidPos position = *player.raidOmenPosition;
    static_cast<void>(createOrExtendRaid(world, player, position, active.amplifier));
    player.raidOmenPosition.reset();
}

Raid* RaidSystem::createOrExtendRaid(world::LoadedWorld& world, PlayerSession& player, const RaidPos& position,
                                     std::int32_t amplifier) {
    // parity: `gameplay/can_start_raid`, which only the Nether's dimension type turns off.
    const std::optional<double> raidsAllowed = world.attributeOverride("minecraft:gameplay/can_start_raid");
    if (player.gamemode == Gamemode::Spectator || !world.gameRules().get(world::gameRules::kRaids) ||
        (raidsAllowed.has_value() ? *raidsAllowed == 0.0
                                  : world.declaration().dimension == config::WorldDimension::Nether)) {
        return nullptr;
    }
    static std::unordered_map<std::string, std::int64_t> cooldownEnds;
    static std::mutex cooldownLock;
    const std::int32_t cooldownSeconds = world.state().gameplay().mechanics.raidCooldownSeconds;
    const std::string cooldownKey = world.name() + "/" + player.uuid().toString();
    if (cooldownSeconds > 0) {
        const std::scoped_lock guard{cooldownLock};
        const auto waiting = cooldownEnds.find(cooldownKey);
        if (waiting != cooldownEnds.end() && waiting->second > world.gameTime()) {
            return nullptr;
        }
    }
    std::vector<world::poi::PoiRecord> points;
    world.state().poi().inRange(world::poi::poiTypes::kVillage,
                                world::poi::BlockPos{position[0], position[1], position[2]}, kVillageSearch,
                                world::poi::Occupancy::kIsOccupied, points);
    RaidPos center = position;
    if (!points.empty()) {
        double sumX = 0.0;
        double sumY = 0.0;
        double sumZ = 0.0;
        for (const world::poi::PoiRecord& point : points) {
            sumX += point.pos.x;
            sumY += point.pos.y;
            sumZ += point.pos.z;
        }
        const double scale = 1.0 / static_cast<double>(points.size());
        center = blockOf(sumX * scale, sumY * scale, sumZ * scale);
    }
    entity::RaidManager& manager = world.raids();
    Raid* raid = manager.nearby(center);
    if (raid == nullptr) {
        if (!world.events().allowed(&server::plugins::pluginEventTypes::raidTrigger,
                                    [&](server::plugins::PluginEvent& event) {
                                        plugins::EventSink::player(event, player);
                                        event.set("x", static_cast<std::int64_t>(center[0]));
                                        event.set("y", static_cast<std::int64_t>(center[1]));
                                        event.set("z", static_cast<std::int64_t>(center[2]));
                                    })) {
            return nullptr;
        }
        const std::int32_t id = manager.add(std::make_unique<Raid>(
            center, entity::raidWaves::groupsFor(world.difficulty()), options_.nextSeed ? options_.nextSeed() : 0));
        raid = manager.get(id);
    }
    if (!raid->started || raid->raidOmenLevel < Raid::kMaxOmenLevel) {
        raid->absorbOmen(amplifier);
        if (!raid->hasFirstWaveSpawned() && hooks_.raidTriggered) {
            hooks_.raidTriggered(player);
        }
    }
    manager.dirty = true;
    if (cooldownSeconds > 0) {
        const std::scoped_lock guard{cooldownLock};
        cooldownEnds[cooldownKey] = world.gameTime() + (static_cast<std::int64_t>(cooldownSeconds) * 20);
    }
    logger_.debug([&] {
        return "Raid at " + std::to_string(raid->center[0]) + ", " + std::to_string(raid->center[1]) + ", " +
               std::to_string(raid->center[2]) + " in world '" + world.name() + "', omen level " +
               std::to_string(raid->raidOmenLevel);
    });
    return raid;
}

} // namespace basalt::server::runtime
