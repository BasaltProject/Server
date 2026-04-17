#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/MobSteps.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
namespace mth = wire::common::mth;
namespace steps = entity::mobSteps;

/// A move this long is a teleport, which makes no sound and starts no fall.
constexpr double kTeleportSquared = 64.0;
/// parity: `getOnPos` and `getOnPosLegacy`, the squares a hair and a fifth under the feet.
constexpr double kSupportDepth = static_cast<double>(1.0E-5F);
constexpr double kEffectDepth = static_cast<double>(0.2F);

using Pos = std::array<std::int32_t, 3>;

[[nodiscard]] Pos below(const ServerEntity& mob, double depth) {
    return {mth::floor(mob.x), mth::floor(mob.y - depth), mth::floor(mob.z)};
}

} // namespace

void MobActionSystem::movementSound(world::LoadedWorld& world, const ServerEntity& mob, std::string_view sound,
                                    float volume, float pitch) {
    if (sound.empty() || (mob.sourceNbt.has_value() && mob.sourceNbt->getBoolean("Silent"))) {
        return;
    }
    random::RandomSource random = freshRandom();
    const std::int64_t seed = random.nextLong();
    const std::string name = sound.starts_with("minecraft:") ? std::string{sound} : "minecraft:" + std::string{sound};
    for (const player::PlayerSession* watcher : watchersOf(world)) {
        player::playSoundFor(*watcher, name, entity::mobSounds::source(mob), mob.x, mob.y, mob.z, volume, pitch, seed);
    }
}

void MobActionSystem::landMob(world::LoadedWorld& world, ServerEntity& mob) {
    const Pos at = below(mob, kEffectDepth);
    const std::int32_t onState = world.state().blockState(at[0], at[1], at[2]).value_or(0);
    const double safe = mob.attributeWithModifiers("minecraft:safe_fall_distance");
    const double fallen = mob.fallDistance;
    // parity: `LivingEntity.checkFallDamage`'s dust, a cloud of the block landed on.
    const double power = std::max(0.0, std::floor(fallen + 1.0E-6 - safe));
    if (power > 0.0 && !server::registry::blocks::isAir(onState)) {
        double x = mob.x;
        double z = mob.z;
        if (at[0] != mth::floor(mob.x) || at[2] != mth::floor(mob.z)) {
            const double xDiff = x - at[0] - 0.5;
            const double zDiff = z - at[2] - 0.5;
            const double maxDiff = std::max(std::abs(xDiff), std::abs(zDiff));
            x = at[0] + 0.5 + (xDiff / maxDiff * 0.5);
            z = at[2] + 0.5 + (zDiff / maxDiff * 0.5);
        }
        const double scale = std::min(static_cast<double>(0.2F) + (power / 15.0), 2.5);
        wire::protocol::play::LevelParticlesPacket packet{
            .particleTypeId = ExplosionSystem::particleId("block"),
            .x = x,
            .y = mob.y,
            .z = z,
            .count = static_cast<std::int32_t>(150.0 * scale),
            .options = wire::protocol::play::ParticleOptions{
                wire::protocol::play::ParticleOptions::BlockState{.stateId = onState}}};
        packet.setMaxSpeed(0.15F);
        for (const player::PlayerSession* watcher : watchersOf(world)) {
            player::showParticleFor(*watcher, "minecraft:block", packet);
        }
    }
    world::vibrations::emit(world.state(), "hit_ground", mob.x, mob.y, mob.z, world.gameTime(), {.entity = mob.uuid()});
    // parity: `restituteMovementAfterCollisions` with `ShelfMushroomBlock.bounceOn`: a fall faster
    // than gravity onto the shelf mushroom bounces, and the bounce sounds from the block for every
    // entity but an item entity and primed tnt. The bounce itself happens in the colliding move,
    // which this build leaves to the client for players and does not restitute for mobs; the sound
    // is the part a watcher can be told.
    constexpr double kBounceMinFall = 0.08;
    constexpr double kBounceProbeDepth = 0.500001;
    const Pos bounceAt = below(mob, kBounceProbeDepth);
    const std::int32_t bounceState = world.state().blockState(bounceAt[0], bounceAt[1], bounceAt[2]).value_or(0);
    const wire::common::Identifier* bounceBlock = server::registry::blockStateDefinitions::blockOf(bounceState);
    if (!world::fallLanding::isSilentBouncer(mob.type()) && fallen > kBounceMinFall && bounceBlock != nullptr &&
        bounceBlock->path() == "shelf_mushroom" && server::registry::blockMovement::bouncinessOf(bounceState) > 0.0F) {
        random::RandomSource random = freshRandom();
        const std::int64_t seed = random.nextLong();
        for (const player::PlayerSession* watcher : watchersOf(world)) {
            player::playSoundFor(*watcher, "minecraft:block.shelf_mushroom.bounce",
                                 wire::protocol::play::SoundSource::Blocks, bounceAt[0] + 0.5, bounceAt[1] + 0.5,
                                 bounceAt[2] + 0.5, 1.0F, 1.0F, seed);
        }
    }
    // parity: `Block.fallOn` then `causeFallDamage`, which the fall-immune skip whole.
    const bool immune = options_.entityTags != nullptr &&
                        world::realEntityTypeTags::membersOf(*options_.entityTags,
                                                             wire::common::Identifier::vanilla("fall_damage_immune"))
                            .contains(mob.type());
    if (immune) {
        return;
    }
    const world::fallLanding::Landing landing = world::fallLanding::of(onState, false);
    const double distance = (fallen * landing.distanceScale) + landing.extraDistance;
    const std::string_view path = mob.type().path();
    if (path == "breeze" && distance > 3.0) {
        movementSound(world, mob, "entity.breeze.land", 1.0F, 1.0F);
    }
    const bool horse =
        path == "horse" || path == "donkey" || path == "mule" || path == "skeleton_horse" || path == "zombie_horse";
    const bool mount = horse || path == "llama" || path == "trader_llama" || path == "camel" || path == "camel_husk";
    // parity: `LivingEntity.causeFallDamage` hands the fall on before anything, a mount only once hurt.
    if (!mount) {
        propagateFall(world, mob, fallen, landing);
    }
    if (horse && distance > 1.0) {
        movementSound(world, mob, mob.age < 0 ? "entity.horse.land_baby" : "entity.horse.land", 0.4F, 1.0F);
    }
    // parity: `Creeper.causeFallDamage` burns the fuse down by half as much again as the fall.
    if (path == "creeper") {
        const std::int32_t maxSwell = mob.sourceNbt.has_value() ? mob.sourceNbt->getShort("Fuse", 30) : 30;
        mob.swell = std::min(mob.swell + static_cast<std::int32_t>(distance * 1.5), maxSwell - 5);
    }
    const std::int32_t damage =
        world::fallLanding::damage(fallen, landing, safe,
                                   mob.attributeWithModifiers("minecraft:fall_damage_multiplier")) -
        steps::fallDamageReduction(mob.type());
    if (damage <= 0) {
        return;
    }
    const bool llama = path == "llama" || path == "trader_llama";
    if (!horse && !llama) {
        movementSound(world, mob, steps::fallSound(mob.type(), damage), 1.0F, 1.0F);
    }
    // parity: `playBlockFallSound`, the block under the feet at half its volume and three quarters its pitch.
    if (!server::registry::blocks::isAir(onState)) {
        const server::registry::BlockSoundTypeEntry& sound = server::registry::blockSounds::of(onState);
        movementSound(world, mob, sound.fallSound, sound.volume * 0.5F, sound.pitch * 0.75F);
    }
    // parity: `Llama.causeFallDamage` hurts only past six blocks.
    if (llama && fallen < 6.0) {
        return;
    }
    entity::entityCombat::blame(mob, std::nullopt, -1, wire::common::Identifier::vanilla("player"));
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(mob, static_cast<float>(damage), std::nullopt, std::nullopt, hooks_.combat,
                                   landing.stalagmite ? "stalagmite" : "fall");
    if (result.landed) {
        combat_->announceEntityHit(watchersOf(world), mob,
                                   landing.stalagmite ? world::damageTypes::kStalagmite : world::damageTypes::kFall, -1,
                                   result.died);
    }
    if (mount) {
        propagateFall(world, mob, fallen, landing);
    }
}

void MobActionSystem::emitMovement(world::LoadedWorld& world, ServerEntity& mob) {
    const std::optional<std::array<double, 3>> from = std::exchange(mob.emittedFrom, std::array{mob.x, mob.y, mob.z});
    if (!from.has_value() || mob.removed) {
        return;
    }
    const double dx = mob.x - (*from)[0];
    const double dy = mob.y - (*from)[1];
    const double dz = mob.z - (*from)[2];
    if ((dx * dx) + (dy * dy) + (dz * dz) > kTeleportSquared) {
        mob.fallDistance = 0.0;
        return;
    }
    world::WorldState& state = world.state();
    const bool inWater = entity::ai::inWater(state, mob);
    // parity: `Entity.checkFallDamage`, before the steps: water and a ladder forget the fall, and so do
    // slow falling and levitation.
    if (steps::countsFalls(mob.type()) && !mob.dying) {
        const Pos feet{mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)};
        const std::int32_t feetState = state.blockState(feet[0], feet[1], feet[2]).value_or(0);
        const wire::common::Identifier* feetBlock = server::registry::blockStateDefinitions::blockOf(feetState);
        const bool climbing = feetBlock != nullptr && blockTagged("minecraft:climbable", *feetBlock).value_or(false);
        const bool floating = mob.effects.contains(wire::common::Identifier::vanilla("slow_falling")) ||
                              mob.effects.contains(wire::common::Identifier::vanilla("levitation"));
        const bool lava = mob.type().path() == "strider" && feetBlock != nullptr && feetBlock->path() == "lava";
        if (inWater || climbing || lava || (floating && dy <= 0.0)) {
            mob.fallDistance = 0.0;
        } else if (dy < 0.0) {
            mob.fallDistance -= static_cast<double>(static_cast<float>(dy));
        }
        if (mob.onGround) {
            if (mob.fallDistance > 0.0) {
                landMob(world, mob);
            }
            mob.fallDistance = 0.0;
        }
    }
    const steps::Emission emission = steps::emissionOf(mob.type());
    if (emission == steps::Emission::None || mob.vehicleId.has_value()) {
        return;
    }
    // parity: `applyMovementEmissionAndPlaySound`.
    const auto moved = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) * static_cast<double>(0.6F));
    const auto horizontal = static_cast<float>(std::sqrt((dx * dx) + (dz * dz)) * static_cast<double>(0.6F));
    const Pos supporting = below(mob, kSupportDepth);
    const std::int32_t supportingState = state.blockState(supporting[0], supporting[1], supporting[2]).value_or(0);
    const auto climbable = [this](std::int32_t stateId) {
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return block != nullptr && blockTagged("minecraft:climbable", *block).value_or(false);
    };
    mob.moveDist += climbable(supportingState) ? moved : horizontal;
    mob.flyDist += moved;
    if (mob.moveDist <= mob.nextStep || server::registry::blocks::isAir(supportingState)) {
        return;
    }
    const Pos effect = below(mob, kEffectDepth);
    const std::int32_t effectState = state.blockState(effect[0], effect[1], effect[2]).value_or(0);
    const bool sounds = emission == steps::Emission::All;
    const bool events = emission != steps::Emission::None;
    // parity: `vibrationAndSoundEffectsFromBlock`: on the ground or a ladder the square steps.
    const auto sideEffects = [&](const Pos& at, std::int32_t stateId, bool sound, bool vibrate) {
        if (server::registry::blocks::isAir(stateId) || (!mob.onGround && !climbable(stateId))) {
            return false;
        }
        if (sound) {
            stepSound(world, mob, at, stateId);
        }
        if (vibrate) {
            world::vibrations::emit(state, "step", mob.x, mob.y, mob.z, world.gameTime(), {.entity = mob.uuid()});
        }
        return true;
    };
    const bool onlyEffect = supporting == effect;
    bool produced = sideEffects(effect, effectState, sounds, onlyEffect);
    if (!onlyEffect) {
        produced = sideEffects(supporting, supportingState, false, events) || produced;
    }
    if (produced) {
        mob.nextStep = static_cast<float>(static_cast<std::int32_t>(mob.moveDist) + 1);
        return;
    }
    if (!inWater) {
        return;
    }
    mob.nextStep = static_cast<float>(static_cast<std::int32_t>(mob.moveDist) + 1);
    if (sounds) {
        // parity: `waterSwimSound`, as loud as it moves, and its own swim sound.
        const auto speed =
            std::min(1.0F, static_cast<float>(std::sqrt((mob.velocityX * mob.velocityX * static_cast<double>(0.2F)) +
                                                        (mob.velocityY * mob.velocityY) +
                                                        (mob.velocityZ * mob.velocityZ * static_cast<double>(0.2F)))) *
                               0.35F);
        random::RandomSource random = freshRandom();
        const float pitch = 1.0F + ((random.nextFloat() - random.nextFloat()) * 0.4F);
        movementSound(world, mob, steps::swimSound(mob), steps::swimVolume(mob, speed), pitch);
    }
    if (events) {
        world::vibrations::emit(state, "swim", mob.x, mob.y, mob.z, world.gameTime(), {.entity = mob.uuid()});
    }
}

void MobActionSystem::stepSound(world::LoadedWorld& world, ServerEntity& mob, const std::array<std::int32_t, 3>& at,
                                std::int32_t stateId) {
    const std::int32_t above = world.state().blockState(at[0], at[1] + 1, at[2]).value_or(0);
    const wire::common::Identifier* feet = server::registry::blockStateDefinitions::blockOf(
        world.state().blockState(mth::floor(mob.x), mth::floor(mob.y), mth::floor(mob.z)).value_or(0));
    const bool inLava = feet != nullptr && feet->path() == "lava";
    const steps::Tagged tagged = [this](std::string_view tag, std::int32_t state) {
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(state);
        return block != nullptr && blockTagged(tag, *block).value_or(false);
    };
    const steps::Sound sound = steps::ownStep(mob, stateId, above, inLava, tagged).value_or(steps::blockStep(stateId));
    movementSound(world, mob, sound.name, sound.volume, sound.pitch);
    // parity: `walkingStepSound`'s chime off an amethyst block, a second apart at most.
    if (tagged("minecraft:crystal_sound_blocks", stateId) && mob.tickCount >= mob.lastCrystalSoundTick + 20) {
        mob.crystalSoundIntensity *=
            static_cast<float>(std::pow(0.997, static_cast<double>(mob.tickCount - mob.lastCrystalSoundTick)));
        mob.crystalSoundIntensity = std::min(1.0F, mob.crystalSoundIntensity + 0.07F);
        random::RandomSource random = freshRandom();
        const float pitch = 0.5F + (mob.crystalSoundIntensity * random.nextFloat() * 1.2F);
        const float volume = 0.1F + (mob.crystalSoundIntensity * 1.2F);
        movementSound(world, mob, "block.amethyst_block.chime", volume, pitch);
        mob.lastCrystalSoundTick = mob.tickCount;
    }
}

void MobActionSystem::emitPlayerMovement(world::LoadedWorld& world,
                                         const std::vector<player::PlayerSession*>& watchers) {
    world::WorldState& state = world.state();
    const auto blockAt = [&state](const Pos& at) { return state.blockState(at[0], at[1], at[2]).value_or(0); };
    const auto tagged = [this](std::string_view tag, std::int32_t stateId) {
        const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return block != nullptr && blockTagged(tag, *block).value_or(false);
    };
    for (player::PlayerSession* player : watchers) {
        const std::optional<std::array<double, 3>> from =
            std::exchange(player->emittedFrom, std::array{player->x, player->y, player->z});
        if (!from.has_value() || player->dead) {
            continue;
        }
        const double dx = player->x - (*from)[0];
        const double dy = player->y - (*from)[1];
        const double dz = player->z - (*from)[2];
        player->knownMovement = {dx, dy, dz};
        // parity: `Player.getMovementEmission`: nothing flying or crouched on the ground, and a
        // spectator's `noPhysics` move emits nothing either.
        if ((dx * dx) + (dy * dy) + (dz * dz) > kTeleportSquared || player->flying ||
            (player->onGround && player->sneaking) || player->gamemode == wire::protocol::play::Gamemode::Spectator ||
            player->vehicleId.has_value()) {
            continue;
        }
        const Pos supporting{mth::floor(player->x), mth::floor(player->y - kSupportDepth), mth::floor(player->z)};
        const std::int32_t supportingState = blockAt(supporting);
        const bool climbing = tagged("minecraft:climbable", supportingState);
        const auto moved = static_cast<float>(std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) * static_cast<double>(0.6F));
        const auto horizontal = static_cast<float>(std::sqrt((dx * dx) + (dz * dz)) * static_cast<double>(0.6F));
        player->moveDist += climbing ? moved : horizontal;
        if (player->moveDist <= player->nextStep || server::registry::blocks::isAir(supportingState)) {
            continue;
        }
        const Pos effect{mth::floor(player->x), mth::floor(player->y - kEffectDepth), mth::floor(player->z)};
        const std::int32_t effectState = blockAt(effect);
        const std::int32_t feetState = blockAt({mth::floor(player->x), mth::floor(player->y), mth::floor(player->z)});
        const bool inWater = world::fluids::of(feetState) == world::Submersion::Water;
        random::RandomSource random = freshRandom();
        const auto sound = [&](std::string_view name, float volume, float pitch) {
            const std::int64_t seed = random.nextLong();
            for (const player::PlayerSession* listener : watchers) {
                if (listener != player) {
                    player::playSoundFor(*listener, "minecraft:" + std::string{name},
                                         wire::protocol::play::SoundSource::Players, player->x, player->y, player->z,
                                         volume, pitch, seed);
                }
            }
        };
        // parity: `vibrationAndSoundEffectsFromBlock`, then `Player.playStepSound`.
        const auto sideEffects = [&](const Pos& at, std::int32_t stateId, bool withSound) {
            const bool ladder = tagged("minecraft:climbable", stateId);
            if (server::registry::blocks::isAir(stateId) ||
                !((player->onGround || ladder || (player->sneaking && dy == 0.0)) && !player->swimming)) {
                return false;
            }
            if (!withSound) {
                return true;
            }
            const server::registry::BlockSoundTypeEntry& own = server::registry::blockSounds::of(stateId);
            // parity: in water, `waterSwimSound` scaled by a server player's own motion, which the client
            // moves and never sets, so it is heard at nothing, and the step muffled under it.
            if (inWater) {
                sound(own.stepSound, own.volume * 0.05F, own.pitch * 0.8F);
            } else {
                const Pos above{at[0], at[1] + 1, at[2]};
                const std::int32_t aboveState = blockAt(above);
                const bool inside = tagged("minecraft:inside_step_sound_blocks", aboveState);
                const bool combination = tagged("minecraft:combination_step_sound_blocks", aboveState);
                if (combination) {
                    const server::registry::BlockSoundTypeEntry& top = server::registry::blockSounds::of(aboveState);
                    sound(top.stepSound, top.volume * 0.15F, top.pitch);
                    sound(own.stepSound, own.volume * 0.05F, own.pitch * 0.8F);
                } else if (inside) {
                    const server::registry::BlockSoundTypeEntry& top = server::registry::blockSounds::of(aboveState);
                    sound(top.stepSound, top.volume * 0.15F, top.pitch);
                } else {
                    sound(own.stepSound, own.volume * 0.15F, own.pitch);
                }
            }
            // parity: the amethyst chime, a second apart at most.
            if (tagged("minecraft:crystal_sound_blocks", stateId) &&
                world.gameTime() >= player->lastCrystalSoundTime + 20) {
                player->crystalSoundIntensity *= static_cast<float>(
                    std::pow(0.997, static_cast<double>(world.gameTime() - player->lastCrystalSoundTime)));
                player->crystalSoundIntensity = std::min(1.0F, player->crystalSoundIntensity + 0.07F);
                const float pitch = 0.5F + (player->crystalSoundIntensity * random.nextFloat() * 1.2F);
                sound("block.amethyst_block.chime", 0.1F + (player->crystalSoundIntensity * 1.2F), pitch);
                player->lastCrystalSoundTime = world.gameTime();
            }
            return true;
        };
        const bool onlyEffect = supporting == effect;
        bool produced = sideEffects(effect, effectState, true);
        if (!onlyEffect) {
            produced = sideEffects(supporting, supportingState, false) || produced;
        }
        if (produced || inWater) {
            player->nextStep = static_cast<float>(static_cast<std::int32_t>(player->moveDist) + 1);
        }
        if (!produced && inWater) {
            world::vibrations::emit(state, "swim", player->x, player->y, player->z, world.gameTime(),
                                    {.entity = player->uuid()});
        }
    }
}

} // namespace basalt::server::runtime
