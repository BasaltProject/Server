#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>

#include <algorithm>
#include <array>
#include <memory>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using world::DragonFight;
using world::RespawnStage;
namespace spikes = server::world::terrain::decoration::end::endSpikes;

/// parity: `DragonRespawnStage`'s own numbers.
constexpr std::array<std::int32_t, 3> kSummoningPoint{0, 128, 0};
constexpr std::int32_t kSummonRoarEvent = 3001;
constexpr std::int32_t kPreparingTicks = 100;
constexpr std::int32_t kPillarTicks = 40;
constexpr std::int32_t kSummoningTicks = 100;
constexpr std::int32_t kRoarFromTick = 80;
constexpr std::int32_t kPillarClearRadius = 10;
constexpr float kPillarBlastPower = 5.0F;
constexpr float kCrystalBlastPower = 6.0F;
constexpr double kLevelEventRange = 64.0;
/// parity: the crystals `tryRespawn` looks for.
constexpr std::int32_t kCrystalReach = 3;

/// parity: `Direction.Plane.HORIZONTAL`: north, east, south, west.
constexpr std::array<std::array<std::int32_t, 2>, 4> kHorizontal{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

[[nodiscard]] bool overlapsBlock(const ServerEntity& entity, std::int32_t x, std::int32_t y, std::int32_t z) {
    const double half = entity.width() / 2.0;
    return entity.x + half > x && entity.x - half < x + 1.0 && entity.y + entity.height() > y && entity.y < y + 1.0 &&
           entity.z + half > z && entity.z - half < z + 1.0;
}

} // namespace

std::vector<PlayerSession*> EndFightSystem::watchersIn(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

void EndFightSystem::levelEvent(const std::vector<PlayerSession*>& watchers, std::int32_t event, std::int32_t x,
                                std::int32_t y, std::int32_t z) {
    for (PlayerSession* player : watchers) {
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
            player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(event, x, y, z, 0);
        }
    }
}

void EndFightSystem::tryRespawn(world::LoadedWorld& world) {
    DragonFight* fight = world.dragonFight();
    if (fight == nullptr || !fight->dragonKilled || fight->respawnStage.has_value()) {
        return;
    }
    if (!fight->exitPortalLocation.has_value()) {
        fight->exitPortalLocation = world::endPodium::locate(world.state());
        if (!fight->exitPortalLocation.has_value()) {
            spawnExitPortal(world, *fight, true);
        }
    }
    const std::array<std::int32_t, 3> centre{(*fight->exitPortalLocation)[0], (*fight->exitPortalLocation)[1] + 1,
                                             (*fight->exitPortalLocation)[2]};
    std::vector<wire::common::Uuid> crystals;
    const std::vector<ServerEntity*> entities = world.state().entities();
    for (const auto& [dx, dz] : kHorizontal) {
        const std::int32_t x = centre[0] + (dx * kCrystalReach);
        const std::int32_t z = centre[2] + (dz * kCrystalReach);
        bool any = false;
        for (const ServerEntity* candidate : entities) {
            if (!candidate->removed && world::endCrystals::isCrystal(candidate->type()) &&
                overlapsBlock(*candidate, x, centre[1], z)) {
                crystals.push_back(candidate->uuid());
                any = true;
            }
        }
        if (!any) {
            return;
        }
    }
    // parity: `respawnDragon` first takes the lit portal apart.
    const std::int32_t originY = (*fight->exitPortalLocation)[1];
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    const registry::BlockState* portal = registry::blocks::named("end_portal");
    const registry::BlockState* endStone = registry::blocks::named("end_stone");
    if (endStone != nullptr) {
        for (std::int32_t y = originY - 1; y <= originY + 3; ++y) {
            for (std::int32_t z = -3; z <= 3; ++z) {
                for (std::int32_t x = -3; x <= 3; ++x) {
                    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
                    if (stateId == bedrock || (portal != nullptr && stateId == portal->stateId)) {
                        static_cast<void>(world.state().setBlockState(x, y, z, endStone->stateId));
                    }
                }
            }
        }
    }
    fight->respawnStage = RespawnStage::Start;
    fight->respawnTime = 0;
    spawnExitPortal(world, *fight, false);
    fight->respawnCrystals = std::move(crystals);
}

bool EndFightSystem::control(world::LoadedWorld& world, std::string_view action, std::int32_t value) {
    world::DragonFight* fight = world.dragonFight();
    if (fight == nullptr) {
        return false;
    }
    if (action == "respawn") {
        tryRespawn(world);
    } else if (action == "stage") {
        if (value < 0 || value > static_cast<std::int32_t>(RespawnStage::End)) {
            return false;
        }
        setRespawnStage(world, *fight, static_cast<RespawnStage>(value));
    } else if (action == "portal") {
        spawnExitPortal(world, *fight, value != 0);
    } else if (action == "gateway") {
        spawnNewGateway(world, *fight);
    } else if (action == "crystals") {
        resetSpikeCrystals(world);
    } else if (action == "bar_progress") {
        updateBar(*fight, std::clamp(static_cast<float>(value) / 10000.0F, 0.0F, 1.0F));
    } else if (action == "bar_visible") {
        showBar(*fight, value != 0);
    } else {
        return false;
    }
    return true;
}

void EndFightSystem::setRespawnStage(world::LoadedWorld& world, DragonFight& fight, RespawnStage stage) {
    fight.respawnTime = 0;
    if (stage != RespawnStage::End) {
        fight.respawnStage = stage;
        return;
    }
    fight.respawnStage.reset();
    fight.dragonKilled = false;
    createNewDragon(world, fight);
}

void EndFightSystem::abortRespawn(world::LoadedWorld& world, DragonFight& fight) {
    fight.respawnStage.reset();
    fight.respawnTime = 0;
    resetSpikeCrystals(world);
    spawnExitPortal(world, fight, true);
}

void EndFightSystem::resetSpikeCrystals(world::LoadedWorld& world) {
    for (const auto& spike : spikes::forSeed(world.metadata().seed)) {
        for (ServerEntity* candidate : world.state().entities()) {
            if (candidate->removed || !world::endCrystals::isCrystal(candidate->type())) {
                continue;
            }
            if (candidate->x + 1.0 > spike.centerX - spike.radius &&
                candidate->x - 1.0 < spike.centerX + spike.radius &&
                candidate->z + 1.0 > spike.centerZ - spike.radius &&
                candidate->z - 1.0 < spike.centerZ + spike.radius) {
                world::endCrystals::setInvulnerable(*candidate, false);
                world::endCrystals::setBeamTarget(*candidate, std::nullopt);
            }
        }
    }
}

void EndFightSystem::tickRespawn(world::LoadedWorld& world, DragonFight& fight,
                                 const std::vector<PlayerSession*>& watchers,
                                 const std::vector<ServerEntity*>& crystals) {
    // parity: `respawnTime++` is taken before the stage runs.
    const std::int32_t time = fight.respawnTime++;
    const auto beamAll = [&crystals](std::optional<std::array<std::int32_t, 3>> target) {
        for (ServerEntity* crystal : crystals) {
            world::endCrystals::setBeamTarget(*crystal, target);
        }
    };
    const auto roar = [this, &watchers] {
        levelEvent(watchers, kSummonRoarEvent, kSummoningPoint[0], kSummoningPoint[1], kSummoningPoint[2]);
    };
    switch (*fight.respawnStage) {
    case RespawnStage::Start:
        beamAll(kSummoningPoint);
        setRespawnStage(world, fight, RespawnStage::PreparingToSummonPillars);
        break;
    case RespawnStage::PreparingToSummonPillars:
        if (time < kPreparingTicks) {
            if (time == 0 || time == 50 || time == 51 || time == 52 || time >= 95) {
                roar();
            }
        } else {
            setRespawnStage(world, fight, RespawnStage::SummoningPillars);
        }
        break;
    case RespawnStage::SummoningPillars: {
        const bool startOfBeam = time % kPillarTicks == 0;
        const bool endOfBeam = time % kPillarTicks == kPillarTicks - 1;
        if (!startOfBeam && !endOfBeam) {
            break;
        }
        const auto& all = spikes::forSeed(world.metadata().seed);
        const auto index = static_cast<std::size_t>(time / kPillarTicks);
        if (index < all.size()) {
            if (startOfBeam) {
                beamAll(std::array{all[index].centerX, all[index].height + 1, all[index].centerZ});
            } else {
                rebuildSpike(world, watchers, index);
            }
        } else if (startOfBeam) {
            setRespawnStage(world, fight, RespawnStage::SummoningDragon);
        }
        break;
    }
    case RespawnStage::SummoningDragon:
        if (time >= kSummoningTicks) {
            setRespawnStage(world, fight, RespawnStage::End);
            resetSpikeCrystals(world);
            // parity: the four crystals go off without touching a block, and are gone.
            const world::explosions::Spares everything = [](world::WorldState&, std::int32_t, std::int32_t,
                                                            std::int32_t, std::int32_t) { return true; };
            for (ServerEntity* crystal : crystals) {
                world::endCrystals::setBeamTarget(*crystal, std::nullopt);
                explosions_->blowUp(world, watchers, crystal->x, crystal->y, crystal->z, kCrystalBlastPower,
                                    "end_crystal", crystal->entityId(), everything);
                crystal->removed = true;
            }
        } else if (time >= kRoarFromTick) {
            roar();
        } else if (time == 0) {
            beamAll(kSummoningPoint);
        } else if (time < 5) {
            roar();
        }
        break;
    case RespawnStage::End:
        break;
    }
}

void EndFightSystem::rebuildSpike(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  std::size_t index) {
    const auto& spike = spikes::forSeed(world.metadata().seed)[index];
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t y = spike.height - kPillarClearRadius; y <= spike.height + kPillarClearRadius; ++y) {
        for (std::int32_t z = spike.centerZ - kPillarClearRadius; z <= spike.centerZ + kPillarClearRadius; ++z) {
            for (std::int32_t x = spike.centerX - kPillarClearRadius; x <= spike.centerX + kPillarClearRadius; ++x) {
                static_cast<void>(world.state().setBlockState(x, y, z, air));
            }
        }
    }
    explosions_->blowUp(world, watchers, spike.centerX + 0.5, spike.height, spike.centerZ + 0.5, kPillarBlastPower);
    const server::world::ChunkHeight bounds = config::dimensionFacts(world.declaration().dimension).chunkHeight;
    spikes::placeBlocks(spike, bounds.minY(),
                        [&world](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                            static_cast<void>(world.state().setBlockState(x, y, z, stateId));
                        });
    // parity: `EndSpikeConfiguration(true, [spike], summoning point)`.
    const registry::EntityTypeEntry* entry =
        registry::entityTypes::of(wire::common::Identifier::vanilla("end_crystal"));
    if (entry != nullptr) {
        random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
        auto crystal = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(),
                                                      entry->id, entry->protocolId, spike.centerX + 0.5,
                                                      static_cast<double>(spike.height + 1), spike.centerZ + 0.5,
                                                      random.nextFloat() * 360.0F, 0.0F);
        world::endCrystals::setBeamTarget(*crystal, kSummoningPoint);
        world::endCrystals::setInvulnerable(*crystal, true);
        static_cast<void>(world.state().spawnEntity(std::move(crystal)));
    }
    static_cast<void>(
        world.state().setBlockState(spike.centerX, spike.height, spike.centerZ, registry::blocks::bedrock().stateId));
    static_cast<void>(world::fires::igniteOnImpact(world.state(), spike.centerX, spike.height + 1, spike.centerZ,
                                                   world.settings().rules.tuning.fireTickDelay));
    syncArea(world, spike.centerX - kPillarClearRadius, spike.centerZ - kPillarClearRadius,
             spike.centerX + kPillarClearRadius, spike.centerZ + kPillarClearRadius);
}

} // namespace basalt::server::runtime
