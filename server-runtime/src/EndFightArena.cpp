#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>

namespace basalt::server::runtime {
namespace {

namespace dragons = entity::enderDragon;
namespace play = wire::protocol::play;
using entity::ServerEntity;
using player::PlayerSession;
using world::DragonFight;

/// parity: `EnderDragonFight`'s own numbers.
constexpr std::int32_t kPlayerScanTicks = 20;
constexpr std::int32_t kDragonLostTicks = 1200;
constexpr std::int32_t kCrystalScanTicks = 100;
constexpr double kFightRange = 192.0;
constexpr double kFightCentreY = 128.0;
constexpr std::int32_t kDragonSpawnY = 128;
constexpr double kGatewayDistance = 96.0;
constexpr std::int32_t kGatewayY = 75;
constexpr std::int32_t kArenaChunks = 8;
/// parity: `LevelEvent.ANIMATION_END_GATEWAY_SPAWN`.
constexpr std::int32_t kGatewaySpawnEvent = 3000;
constexpr float kCrystalBlastOnDragon = 10.0F;

[[nodiscard]] bool isLiveDragon(const ServerEntity& entity) {
    return dragons::isDragon(entity.type()) && !entity.removed && entity.health > 0.0;
}

} // namespace

DragonFight* EndFightSystem::fightOf(world::LoadedWorld& world, const ServerEntity& dragon) {
    DragonFight* fight = world.dragonFight();
    return fight != nullptr && fight->dragonUuid == dragon.uuid() ? fight : nullptr;
}

void EndFightSystem::tickFight(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                               const std::vector<ServerEntity*>& living) {
    DragonFight* fight = world.dragonFight();
    if (fight == nullptr) {
        return;
    }
    showBar(*fight, !fight->dragonKilled);
    if (++fight->ticksSinceLastPlayerScan >= kPlayerScanTicks) {
        updatePlayers(world, *fight, watchers);
        fight->ticksSinceLastPlayerScan = 0;
    }
    if (fight->barPlayers.empty()) {
        return;
    }
    // parity: the dragon ticket holds the arena; here it is enough that the chunks are there.
    for (std::int32_t chunkX = -kArenaChunks; chunkX <= kArenaChunks; ++chunkX) {
        static_cast<void>(world.state().getOrGenerateChunk(chunkX, kArenaChunks));
    }
    if (fight->needsStateScanning) {
        scanState(world, *fight, living);
        fight->needsStateScanning = false;
    }
    if (fight->respawnStage.has_value()) {
        std::vector<ServerEntity*> crystals;
        for (ServerEntity* candidate : living) {
            if (!candidate->removed &&
                std::ranges::find(fight->respawnCrystals, candidate->uuid()) != fight->respawnCrystals.end()) {
                crystals.push_back(candidate);
            }
        }
        if (crystals.empty()) {
            abortRespawn(world, *fight);
            return;
        }
        tickRespawn(world, *fight, watchers, crystals);
    }
    if (fight->dragonKilled) {
        return;
    }
    if (!fight->dragonUuid.has_value() || ++fight->ticksSinceDragonSeen >= kDragonLostTicks) {
        findOrCreateDragon(world, *fight, living);
        fight->ticksSinceDragonSeen = 0;
    }
    if (++fight->ticksSinceCrystalsScanned >= kCrystalScanTicks) {
        updateCrystalCount(world, *fight);
        fight->ticksSinceCrystalsScanned = 0;
    }
}

void EndFightSystem::updatePlayers(world::LoadedWorld& world, DragonFight& fight,
                                   const std::vector<PlayerSession*>& watchers) {
    std::set<std::int32_t> near;
    for (const PlayerSession* player : watchers) {
        if (player->dead || player->worldName != world.name()) {
            continue;
        }
        const double dy = player->y - kFightCentreY;
        if ((player->x * player->x) + (dy * dy) + (player->z * player->z) <= kFightRange * kFightRange) {
            near.insert(player->entityId());
        }
    }
    const auto send = [this, &fight](std::int32_t entityId, play::BossEventPacket::Operation operation) {
        for (PlayerSession* player : players_->snapshot()) {
            if (player->entityId() != entityId) {
                continue;
            }
            if (player->connection().showBossBar) {
                const auto* add = std::get_if<play::BossEventPacket::Add>(&operation);
                player->connection().showBossBar(fight.barId,
                                                 add != nullptr ? add->name : wire::text::Component::empty(),
                                                 fight.barProgress, add != nullptr);
            } else if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(play::BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    play::BossEventPacket{.id = fight.barId, .operation = operation}.write(buffer);
                });
            }
        }
    };
    const play::BossEventPacket::Add add{.name = wire::text::Component::translatable("entity.minecraft.ender_dragon"),
                                         .progress = fight.barProgress,
                                         .color = play::BossBarColor::Pink,
                                         .overlay = play::BossBarOverlay::Progress,
                                         .darkenScreen = false,
                                         .playMusic = true,
                                         .createWorldFog = true};
    for (const std::int32_t joined : near) {
        if (!fight.barPlayers.contains(joined) && fight.barVisible) {
            send(joined, add);
        }
    }
    for (const std::int32_t left : fight.barPlayers) {
        if (!near.contains(left) && fight.barVisible) {
            send(left, play::BossEventPacket::Remove{});
        }
    }
    fight.barPlayers = std::move(near);
}

void EndFightSystem::showBar(DragonFight& fight, bool visible) {
    if (fight.barVisible == visible) {
        return;
    }
    fight.barVisible = visible;
    for (PlayerSession* player : players_->snapshot()) {
        if (!fight.barPlayers.contains(player->entityId())) {
            continue;
        }
        if (player->connection().showBossBar) {
            player->connection().showBossBar(fight.barId,
                                             wire::text::Component::translatable("entity.minecraft.ender_dragon"),
                                             fight.barProgress, visible);
            continue;
        }
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        const play::BossEventPacket::Operation operation =
            visible ? play::BossEventPacket::Operation{play::BossEventPacket::Add{
                          .name = wire::text::Component::translatable("entity.minecraft.ender_dragon"),
                          .progress = fight.barProgress,
                          .color = play::BossBarColor::Pink,
                          .overlay = play::BossBarOverlay::Progress,
                          .darkenScreen = false,
                          .playMusic = true,
                          .createWorldFog = true}}
                    : play::BossEventPacket::Operation{play::BossEventPacket::Remove{}};
        player->connection().sendPlayPacket(play::BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::BossEventPacket{.id = fight.barId, .operation = operation}.write(buffer);
        });
    }
}

void EndFightSystem::updateBar(DragonFight& fight, float progress) {
    if (fight.barProgress == progress) {
        return;
    }
    fight.barProgress = progress;
    if (!fight.barVisible) {
        return;
    }
    for (PlayerSession* player : players_->snapshot()) {
        if (!fight.barPlayers.contains(player->entityId())) {
            continue;
        }
        if (player->connection().showBossBar) {
            player->connection().showBossBar(fight.barId, wire::text::Component::empty(), progress, true);
        } else if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                play::BossEventPacket{.id = fight.barId, .operation = play::BossEventPacket::UpdateProgress{progress}}
                    .write(buffer);
            });
        }
    }
}

void EndFightSystem::scanState(world::LoadedWorld& world, DragonFight& fight,
                               const std::vector<ServerEntity*>& living) {
    const std::optional<std::array<std::int32_t, 3>> podium = world::endPodium::locate(world.state());
    const bool active = podium.has_value() && world::endPodium::isActive(world.state(), (*podium)[1]);
    if (podium.has_value() && !fight.exitPortalLocation.has_value()) {
        fight.exitPortalLocation = podium;
    }
    if (active) {
        fight.previouslyKilled = true;
    } else {
        fight.previouslyKilled = false;
        if (!podium.has_value()) {
            spawnExitPortal(world, fight, false);
        }
    }
    const auto first =
        std::ranges::find_if(living, [](const ServerEntity* candidate) { return isLiveDragon(*candidate); });
    if (first == living.end()) {
        fight.dragonKilled = true;
    } else {
        fight.dragonUuid = (*first)->uuid();
        fight.dragonKilled = false;
        // parity: a dragon in a world without a lit portal is one the fight did not make.
        if (!active) {
            (*first)->removed = true;
            fight.dragonUuid.reset();
        }
    }
    if (!fight.previouslyKilled && fight.dragonKilled) {
        fight.dragonKilled = false;
    }
}

void EndFightSystem::findOrCreateDragon(world::LoadedWorld& world, DragonFight& fight,
                                        const std::vector<ServerEntity*>& living) {
    const auto first =
        std::ranges::find_if(living, [](const ServerEntity* candidate) { return isLiveDragon(*candidate); });
    if (first != living.end()) {
        fight.dragonUuid = (*first)->uuid();
        return;
    }
    createNewDragon(world, fight);
}

void EndFightSystem::createNewDragon(world::LoadedWorld& world, DragonFight& fight) {
    const registry::EntityTypeEntry* entry =
        registry::entityTypes::of(wire::common::Identifier::vanilla("ender_dragon"));
    if (entry == nullptr) {
        return;
    }
    static_cast<void>(world.state().getOrGenerateChunk(0, 0));
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const float yaw = random.nextFloat() * 360.0F;
    auto dragon = std::make_unique<ServerEntity>(players_->allocateEntityIds(dragons::idSpan(entry->id)),
                                                 wire::common::Uuid::random(), entry->id, entry->protocolId, 0.0,
                                                 static_cast<double>(kDragonSpawnY), 0.0, yaw, 0.0F);
    dragon->health = dragons::kMaxHealth;
    dragon->persistent = true;
    dragon->noGravity = true;
    dragon->dragon = std::make_shared<dragons::DragonState>(dragons::freshState(*dragon));
    dragon->dragon->random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    // parity: `createNewDragon` starts it circling.
    dragons::setPhase(*dragon->dragon, dragons::Phase::HoldingPattern);
    fight.dragonUuid = dragon->uuid();
    static_cast<void>(world.state().spawnEntity(std::move(dragon)));
}

void EndFightSystem::updateCrystalCount(world::LoadedWorld& world, DragonFight& fight) {
    fight.ticksSinceCrystalsScanned = 0;
    fight.aliveCrystals = 0;
    const auto& spikes = server::world::terrain::decoration::end::endSpikes::forSeed(world.metadata().seed);
    for (const ServerEntity* candidate : world.state().entities()) {
        if (candidate->removed || !world::endCrystals::isCrystal(candidate->type())) {
            continue;
        }
        // parity: `EndSpike.getTopBoundingBox`, the pillar's square at every height.
        for (const auto& spike : spikes) {
            if (candidate->x + 1.0 > spike.centerX - spike.radius &&
                candidate->x - 1.0 < spike.centerX + spike.radius &&
                candidate->z + 1.0 > spike.centerZ - spike.radius &&
                candidate->z - 1.0 < spike.centerZ + spike.radius) {
                fight.aliveCrystals++;
                break;
            }
        }
    }
}

void EndFightSystem::updateDragon(world::LoadedWorld& world, const ServerEntity& dragon) {
    DragonFight* fight = fightOf(world, dragon);
    if (fight == nullptr) {
        return;
    }
    updateBar(*fight, static_cast<float>(dragon.health / dragons::kMaxHealth));
    fight->ticksSinceDragonSeen = 0;
}

void EndFightSystem::dragonKilled(world::LoadedWorld& world, const ServerEntity& dragon) {
    DragonFight* fight = fightOf(world, dragon);
    if (fight == nullptr) {
        return;
    }
    updateBar(*fight, 0.0F);
    showBar(*fight, false);
    spawnExitPortal(world, *fight, true);
    spawnNewGateway(world, *fight);
    if (!fight->previouslyKilled) {
        const server::world::Chunk& chunk = world.state().getOrGenerateChunk(0, 0);
        const std::int32_t y = chunk.heightAt(0, 0, server::world::HeightmapType::MotionBlocking);
        if (const registry::BlockState* egg = registry::blocks::named("dragon_egg"); egg != nullptr) {
            static_cast<void>(world.state().setBlockState(0, y, 0, egg->stateId));
            syncArea(world, 0, 0, 0, 0);
        }
    }
    fight->previouslyKilled = true;
    fight->dragonKilled = true;
}

void EndFightSystem::spawnExitPortal(world::LoadedWorld& world, DragonFight& fight, bool active) {
    if (!fight.exitPortalLocation.has_value()) {
        fight.exitPortalLocation = std::array<std::int32_t, 3>{0, world::endPodium::exitPortalY(world.state()), 0};
    }
    world::endPodium::placeAt(world.state(), (*fight.exitPortalLocation)[1], active);
    syncArea(world, -4, -4, 4, 4);
}

void EndFightSystem::spawnNewGateway(world::LoadedWorld& world, DragonFight& fight) {
    if (fight.gateways.empty()) {
        return;
    }
    const std::int32_t gateway = fight.gateways.back();
    fight.gateways.pop_back();
    const double angle = 2.0 * (-std::numbers::pi + ((std::numbers::pi / DragonFight::kGatewayCount) * gateway));
    const std::int32_t x = wire::common::mth::floor(kGatewayDistance * std::cos(angle));
    const std::int32_t z = wire::common::mth::floor(kGatewayDistance * std::sin(angle));
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name() && player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(kGatewaySpawnEvent, x, kGatewayY, z, 0);
        }
    }
    // parity: `END_GATEWAY_DELAYED`, a gateway with no exit yet, capped with bedrock above and below.
    world::endGateways::place(world.state(), x, kGatewayY, z, std::nullopt, false);
    syncArea(world, x - 1, z - 1, x + 1, z + 1);
}

void EndFightSystem::syncArea(world::LoadedWorld& world, std::int32_t minX, std::int32_t minZ, std::int32_t maxX,
                              std::int32_t maxZ) {
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName != world.name() || !player->connection().resendChunk) {
            continue;
        }
        for (std::int32_t chunkX = minX >> 4; chunkX <= (maxX >> 4); ++chunkX) {
            for (std::int32_t chunkZ = minZ >> 4; chunkZ <= (maxZ >> 4); ++chunkZ) {
                player->connection().resendChunk(chunkX, chunkZ);
            }
        }
    }
}

void EndFightSystem::crystalDestroyed(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                      const ServerEntity& crystal, std::optional<std::int32_t> byPlayer) {
    DragonFight* fight = world.dragonFight();
    if (fight == nullptr) {
        return;
    }
    // parity: a respawn crystal lost stops the respawn, and nothing else happens.
    if (fight->respawnStage.has_value() &&
        std::ranges::find(fight->respawnCrystals, crystal.uuid()) != fight->respawnCrystals.end()) {
        abortRespawn(world, *fight);
        return;
    }
    updateCrystalCount(world, *fight);
    ServerEntity* dragon = nullptr;
    for (ServerEntity* candidate : world.state().entities()) {
        if (candidate->dragon != nullptr && fight->dragonUuid == candidate->uuid()) {
            dragon = candidate;
            break;
        }
    }
    if (dragon == nullptr || dragon->removed) {
        return;
    }
    // parity: whoever broke it, or else the nearest player the dragon could fight.
    std::optional<dragons::Target> player;
    double nearest = std::numeric_limits<double>::max();
    for (const PlayerSession* candidate : watchers) {
        if (candidate->dead || candidate->gamemode == play::Gamemode::Creative ||
            candidate->gamemode == play::Gamemode::Spectator) {
            continue;
        }
        const double dx = candidate->x - crystal.x;
        const double dy = candidate->y - crystal.y;
        const double dz = candidate->z - crystal.z;
        const double distance = (dx * dx) + (dy * dy) + (dz * dz);
        const bool chosen = byPlayer.has_value() ? candidate->entityId() == *byPlayer : distance < nearest;
        if (chosen) {
            nearest = distance;
            player = dragons::Target{.entityId = candidate->entityId(),
                                     .x = candidate->x,
                                     .y = candidate->y,
                                     .z = candidate->z,
                                     .height = 1.8};
        }
    }
    dragons::DragonState& state = stateOf(*dragon);
    if (state.nearestCrystal == crystal.entityId()) {
        state.hitPart = dragons::Part::Head;
        const entity::entityCombat::Result result = entity::entityCombat::hurt(
            *dragon, kCrystalBlastOnDragon, crystal.x, crystal.z, {}, "explosion", std::nullopt, player.has_value());
        if (result.landed) {
            combat_->announceEntityHit(watchers, *dragon, wire::common::Identifier::vanilla("explosion"),
                                       crystal.entityId(), result.died);
        }
    }
    if (player.has_value()) {
        dragons::DragonWorld view = worldFor(world, watchers, *dragon, state);
        dragons::strafeAfterCrystal(*dragon, state, view, *player);
    }
}

} // namespace basalt::server::runtime
