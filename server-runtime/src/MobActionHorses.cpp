#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::SoundSource;
namespace horses = entity::horses;

/// parity: `AbstractHorse.getSoundVolume`.
constexpr float kHorseVolume = 0.8F;

} // namespace

void MobActionSystem::tameHorse(ServerEntity& horse, std::int32_t playerId) {
    world::LoadedWorld* world = worldOf(horse);
    if (world == nullptr) {
        return;
    }
    for (PlayerSession* player : watchersOf(*world)) {
        if (player->entityId() != playerId) {
            continue;
        }
        horses::tame(horse, player->uuid());
        if (options_.advancementTriggers != nullptr) {
            options_.advancementTriggers->fire(
                "tame_animal", player->name(), player->uuid().toString(),
                world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(*world, horse)}}});
        }
        entityEvent(horse, horses::kEventTamingSucceeded);
        return;
    }
}

void MobActionSystem::throwRiders(ServerEntity& horse) {
    world::LoadedWorld* world = worldOf(horse);
    if (world == nullptr) {
        return;
    }
    // parity: `ejectPassengers`, the players through their own dismount.
    const std::vector<std::int32_t> riders = horse.passengerIds;
    const std::vector<PlayerSession*> watchers = watchersOf(*world);
    bool mobs = false;
    for (const std::int32_t rider : riders) {
        const auto player =
            std::ranges::find_if(watchers, [rider](const PlayerSession* one) { return one->entityId() == rider; });
        if (player != watchers.end()) {
            if ((*player)->connection().dismount) {
                (*player)->connection().dismount();
            }
            continue;
        }
        if (ServerEntity* mob = world->state().entity(rider); mob != nullptr) {
            mob->vehicleId.reset();
        }
        std::erase(horse.passengerIds, rider);
        mobs = true;
    }
    if (mobs) {
        const wire::protocol::play::SetPassengersPacket packet{.vehicleId = horse.entityId(),
                                                               .passengerIds = horse.passengerIds};
        for (PlayerSession* watcher : watchers) {
            if (watcher->connection().sendPlayPacket) {
                watcher->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                     [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    }
    makeHorseMad(horse);
    entityEvent(horse, horses::kEventTamingFailed);
}

void MobActionSystem::makeHorseMad(ServerEntity& horse) {
    world::LoadedWorld* world = worldOf(horse);
    if (world == nullptr || (horse.horse != nullptr && horse.horse->standing)) {
        return;
    }
    horses::standIfPossible(horse);
    if (const std::string_view sound = horses::angrySound(horse); !sound.empty() && !horses::silent(horse)) {
        random::RandomSource random = freshRandom();
        const float roll = random.nextFloat() - random.nextFloat();
        playSound(*world, horse, sound, kHorseVolume, horses::voicePitch(horse, roll), SoundSource::Neutral);
    }
}

void MobActionSystem::horseRears(ServerEntity& horse) {
    world::LoadedWorld* world = worldOf(horse);
    if (world == nullptr) {
        return;
    }
    horses::standIfPossible(horse);
    // parity: `RandomStandGoal.playStandSound`, the ambient sound at the mob's own volume and pitch.
    if (const std::string_view sound = horses::ambientSound(horse); !sound.empty() && !horses::silent(horse)) {
        random::RandomSource random = freshRandom();
        const float roll = random.nextFloat() - random.nextFloat();
        playSound(*world, horse, sound, kHorseVolume, horses::voicePitch(horse, roll), SoundSource::Neutral);
    }
}

void MobActionSystem::tickHorse(world::LoadedWorld& world, ServerEntity& horse, random::RandomSource& random) {
    constexpr std::int32_t kTailOdds = 200;
    constexpr std::int32_t kHealOdds = 900;
    constexpr std::int32_t kGrazeOdds = 300;
    constexpr std::int32_t kGrazeTicks = 50;
    constexpr std::int32_t kMouthTicks = 30;
    horses::HorseState& state = horses::stateOf(horse);
    // parity: `moveTail`'s roll, which only the client draws.
    static_cast<void>(random.nextInt(kTailOdds));
    if (horse.health > 0.0) {
        if (random.nextInt(kHealOdds) == 0 && horse.deathTime == 0) {
            horse.health = std::min(horse.health + 1.0, horse.attributeWithModifiers("minecraft:max_health"));
        }
        if (horses::canEatGrass(horse.type())) {
            if (!state.eating && horse.passengerIds.empty() && random.nextInt(kGrazeOdds) == 0) {
                const std::optional<std::int32_t> below = world.state().blockState(
                    static_cast<std::int32_t>(std::floor(horse.x)), static_cast<std::int32_t>(std::floor(horse.y)) - 1,
                    static_cast<std::int32_t>(std::floor(horse.z)));
                const std::optional<server::registry::BlockState> block =
                    below.has_value() ? server::registry::blocks::byStateId(*below) : std::nullopt;
                if (block.has_value() && block->block.path() == "grass_block") {
                    state.eating = true;
                }
            }
            if (state.eating && ++state.eatingCounter > kGrazeTicks) {
                state.eatingCounter = 0;
                state.eating = false;
            }
        }
    }
    if (state.mouthCounter > 0 && ++state.mouthCounter > kMouthTicks) {
        state.mouthCounter = 0;
        state.mouthOpen = false;
    }
    if (state.standCounter > 0 && --state.standCounter <= 0) {
        state.standing = false;
        state.standCounter = 0;
    }
    if (!horses::isCamel(horse.type())) {
        return;
    }
    // parity: `Camel.tick`: the dash ends on landing, and the cooldown ends with its ready sound.
    constexpr std::int32_t kDashLands = 50;
    if (state.dashing && state.dashCooldown < kDashLands && (horse.onGround || horse.vehicleId.has_value())) {
        state.dashing = false;
    }
    if (state.dashCooldown > 0 && --state.dashCooldown == 0) {
        playSound(world, horse,
                  horse.type().path() == "camel_husk" ? "minecraft:entity.camel_husk.dash_ready"
                                                      : "minecraft:entity.camel.dash_ready",
                  1.0F, 1.0F, SoundSource::Neutral);
    }
}

} // namespace basalt::server::runtime
