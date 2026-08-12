#include "CombatSystemShared.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>

namespace basalt::server::runtime {
using namespace combatSystem;

bool CombatSystem::deflected(const std::vector<PlayerSession*>& watchers, const std::vector<ServerEntity*>& living,
                             ServerEntity& projectile, double fromX, double fromY, double fromZ) {
    // parity: `Breeze.deflection`, which lets a wind charge through.
    const std::string_view path = projectile.type().path();
    if (path == "wind_charge" || path == "breeze_wind_charge" || options_.entityTags == nullptr) {
        return false;
    }
    const std::set<Identifier>& deflectors =
        world::realEntityTypeTags::membersOf(*options_.entityTags, Identifier::vanilla("deflects_projectiles"));
    if (deflectors.empty() || std::ranges::none_of(living, [&deflectors](const ServerEntity* mob) {
            return deflectors.contains(mob->type());
        })) {
        return false;
    }
    const std::optional<Target> target = targetAlong(watchers, living, projectile, fromX, fromY, fromZ);
    if (!target.has_value() || target->mob == nullptr || !deflectors.contains(target->mob->type())) {
        return false;
    }
    // parity: `hitTargetOrDeflectSelf`.
    const ServerEntity& breeze = *target->mob;
    if (projectile.lastDeflectedBy == breeze.entityId()) {
        return true;
    }
    projectile.lastDeflectedBy = breeze.entityId();
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    for (const PlayerSession* watcher : watchers) {
        player::playSoundFor(*watcher, "minecraft:entity.breeze.deflect", wire::protocol::play::SoundSource::Hostile,
                             breeze.x, breeze.y, breeze.z, 1.0F, 1.0F, seed);
    }
    // parity: `ProjectileDeflection.REVERSE`.
    constexpr double kReverse = -0.5;
    projectile.velocityX *= kReverse;
    projectile.velocityY *= kReverse;
    projectile.velocityZ *= kReverse;
    projectile.yaw += 170.0F + (static_cast<float>(options_.nextInt ? options_.nextInt(1000) : 0) / 1000.0F * 20.0F);
    return true;
}

void CombatSystem::landEnderPearl(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  ServerEntity& pearl) {
    pearl.removed = true;
    if (!pearl.ownerId.has_value()) {
        return;
    }
    PlayerSession* owner = nullptr;
    for (PlayerSession* candidate : watchers) {
        if (candidate->entityId() == *pearl.ownerId) {
            owner = candidate;
            break;
        }
    }
    if (owner == nullptr || owner->dead) {
        return;
    }
    broadcaster_.sendEffects(watchers, static_cast<std::int32_t>(std::floor(pearl.x)),
                             static_cast<std::int32_t>(std::floor(pearl.y)),
                             static_cast<std::int32_t>(std::floor(pearl.z)), server::registry::blocks::air().stateId,
                             {world::blockEffect::Sound{.name = std::string{kSoundTeleport},
                                                        .source = wire::protocol::play::SoundSource::Players,
                                                        .volume = 1.0F,
                                                        .pitch = 1.0F}});
    // parity: `ThrownEnderpearl.onHit`, a chance of an endermite at the thrower, before the teleport.
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    if (random.nextFloat() < world.state().gameplay().items.enderPearlEndermiteSpawnChance && options_.spawnMob &&
        world.gameRules().get(world::gameRules::kSpawnMonsters) && world.difficulty() != "peaceful") {
        options_.spawnMob(world, Identifier::vanilla("endermite"), owner->x, owner->y, owner->z, owner->yaw);
    }
    if (owner->connection().teleportTo) {
        owner->connection().teleportTo(pearl.x, pearl.y, pearl.z, owner->yaw, owner->pitch);
    }
    // parity: `ThrownEnderpearl.onHit` lets go of the thrower's impulse.
    player::impulse::reset(*owner);
    if (owner->connection().hurt) {
        owner->connection().hurt(world.state().gameplay().items.enderPearlDamage, kDeathFall,
                                 world::damageTypes::kEnderPearl, std::nullopt);
    }
}

bool CombatSystem::firedByPlayer(const std::vector<PlayerSession*>& watchers, const ServerEntity& projectile) {
    return projectile.ownerId.has_value() && std::ranges::any_of(watchers, [&projectile](const PlayerSession* player) {
               return player->entityId() == *projectile.ownerId;
           });
}

void CombatSystem::rememberShooter(const std::vector<PlayerSession*>& watchers, const ServerEntity& projectile,
                                   ServerEntity& target) {
    for (const PlayerSession* player : watchers) {
        if (projectile.ownerId == player->entityId()) {
            entity::entityCombat::rememberPlayer(target, player->uuid());
        }
    }
}

std::optional<CombatSystem::Target> CombatSystem::targetAlong(const std::vector<PlayerSession*>& watchers,
                                                              const std::vector<ServerEntity*>& living,
                                                              const ServerEntity& projectile, double fromX,
                                                              double fromY, double fromZ, double margin,
                                                              const std::vector<std::int32_t>* spared) {
    if (projectile.forcedHitId.has_value()) {
        for (ServerEntity* candidate : living) {
            if (candidate->entityId() == *projectile.forcedHitId) {
                return Target{.mob = candidate, .player = nullptr};
            }
        }
        for (PlayerSession* candidate : watchers) {
            if (candidate->entityId() == *projectile.forcedHitId) {
                return Target{.mob = nullptr, .player = candidate};
            }
        }
    }
    double nearest = std::numeric_limits<double>::max();
    Target found;
    // parity: `Projectile.canHitEntity`; without `spared` the owner alone is left out.
    const auto sparedId = [&projectile, spared](std::int32_t id) {
        return spared != nullptr ? std::ranges::contains(*spared, id)
                                 : projectile.ownerId.has_value() && id == *projectile.ownerId;
    };
    for (ServerEntity* candidate : living) {
        if (candidate->entityId() == projectile.entityId() || sparedId(candidate->entityId()) ||
            std::ranges::contains(projectile.piercedIds, candidate->entityId())) {
            continue;
        }
        // parity: `Projectile.canHitEntity`.
        if (!server::registry::entityAttributes::isLiving(candidate->type()) &&
            !world::endCrystals::isCrystal(candidate->type()) && !world::detectorRails::isMinecart(candidate->type()) &&
            !entity::boats::isBoat(candidate->type())) {
            continue;
        }
        // parity: `EnderDragon.isPickable` is false; what a projectile meets is one of its parts.
        if (candidate->dragon != nullptr) {
            for (std::size_t part = 0; part < entity::enderDragon::kParts; ++part) {
                const entity::enderDragon::Vec3& centre = candidate->dragon->parts[part];
                const entity::enderDragon::PartBox& box = entity::enderDragon::kPartBoxes[part];
                const std::optional<double> at =
                    clip(projectile, fromX, fromY, fromZ, centre.x, centre.y, centre.z, static_cast<double>(box.width),
                         static_cast<double>(box.height), margin);
                if (at.has_value() && *at < nearest) {
                    nearest = *at;
                    found = Target{.mob = candidate,
                                   .player = nullptr,
                                   .dragonPart = static_cast<entity::enderDragon::Part>(part)};
                }
            }
            continue;
        }
        const std::optional<double> at = clip(projectile, fromX, fromY, fromZ, candidate->x, candidate->y, candidate->z,
                                              candidate->width(), candidate->height(), margin);
        if (!at.has_value() || *at >= nearest) {
            continue;
        }
        nearest = *at;
        found = Target{.mob = candidate, .player = nullptr};
    }
    for (PlayerSession* candidate : watchers) {
        if (sparedId(candidate->entityId()) || candidate->dead ||
            std::ranges::contains(projectile.piercedIds, candidate->entityId())) {
            continue;
        }
        if (candidate->gamemode == Gamemode::Creative || candidate->gamemode == Gamemode::Spectator) {
            continue;
        }
        const std::optional<double> at = clip(projectile, fromX, fromY, fromZ, candidate->x, candidate->y, candidate->z,
                                              server::registry::entityTypeShapes::widthOf(playerType()),
                                              server::registry::entityTypeShapes::heightOf(playerType()), margin);
        if (!at.has_value() || *at >= nearest) {
            continue;
        }
        nearest = *at;
        found = Target{.mob = nullptr, .player = candidate};
    }
    if (found.mob == nullptr && found.player == nullptr) {
        return std::nullopt;
    }
    return found;
}

void CombatSystem::hitCrystal(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                              ServerEntity& crystal, const bool byExplosion, const bool byDragon,
                              const std::optional<std::int32_t> byPlayer, const bool byCreative) {
    const world::endCrystals::Hit hit = world::endCrystals::hurt(crystal, byDragon, byExplosion, byCreative);
    if (hit == world::endCrystals::Hit::Ignored) {
        return;
    }
    if (hit == world::endCrystals::Hit::Explodes) {
        const config::GameplayRules::Blocks& rules = world.state().gameplay().blocks;
        const bool based = world::endCrystals::showsBottom(crystal);
        if (based ? rules.endCrystalBaseExplode : rules.endCrystalBaselessExplode) {
            explosions_.blowUp(world, watchers, crystal.x, crystal.y, crystal.z,
                               based ? rules.endCrystalBaseExplosionPower : rules.endCrystalBaselessExplosionPower,
                               "end_crystal", crystal.entityId(), {}, {},
                               based ? rules.endCrystalBaseExplosionFire : rules.endCrystalBaselessExplosionFire);
        }
        emitVibration(world, "explode", crystal.x, crystal.y, crystal.z, crystal.uuid());
    }
    // parity: `EndCrystal.onDestroyedBy`, after the blast, whatever took it.
    if (options_.crystalDestroyed) {
        options_.crystalDestroyed(world, watchers, crystal, byPlayer);
    }
}

void CombatSystem::hurtByExplosion(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                   const double x, const double y, const double z, const float power,
                                   const std::int32_t sourceEntityId) {
    const double reach = static_cast<double>(power) * 2.0;
    for (ServerEntity* entity : world.state().entities()) {
        if (entity->removed || entity->dying || entity->entityId() == sourceEntityId) {
            continue;
        }
        const double distance = std::sqrt(((entity->x - x) * (entity->x - x)) + ((entity->y - y) * (entity->y - y)) +
                                          ((entity->z - z) * (entity->z - z)));
        if (distance > reach) {
            continue;
        }
        if (world::endCrystals::isCrystal(entity->type())) {
            hitCrystal(world, watchers, *entity, true);
            continue;
        }
        // parity: `Warden.ignoreExplosion` while it is rising or burrowing.
        if (entity::wardenBrain::isDiggingOrEmerging(*entity)) {
            continue;
        }
        if (ExperienceOrbSystem::isOrb(*entity)) {
            ExperienceOrbSystem::hurt(*entity, world::explosions::damageAt(distance, power, static_cast<float>(reach)));
            continue;
        }
        // parity: a vehicle takes the blast like anything else, unless a mob set it off with griefing off.
        if (world::detectorRails::isMinecart(entity->type()) || entity::boats::isBoat(entity->type())) {
            const ServerEntity* source = world.state().entity(sourceEntityId);
            const ServerEntity* behind =
                source != nullptr && source->ownerId.has_value() ? world.state().entity(*source->ownerId) : source;
            const bool mobSource = behind != nullptr && server::registry::entityAttributes::isLiving(behind->type());
            const float damage = world::explosions::damageAt(distance, power, static_cast<float>(reach));
            if (options_.hurtVehicle && !VehicleSystem::ignoresExplosion(world, mobSource) && damage > 0.0F) {
                static_cast<void>(options_.hurtVehicle(
                    world, *entity,
                    VehicleSystem::Hit{.amount = damage,
                                       .type = world::damageTypes::kExplosion,
                                       .causeId = behind == nullptr ? std::nullopt : std::optional{behind->entityId()},
                                       .directId =
                                           source == nullptr ? std::nullopt : std::optional{source->entityId()}}));
            }
            continue;
        }
        if (!server::registry::entityAttributes::isLiving(entity->type())) {
            continue;
        }
        const float damage = world::explosions::damageAt(distance, power, static_cast<float>(reach));
        if (damage <= 0.0F) {
            continue;
        }
        markWitherFriend(world, *entity, sourceEntityId);
        // parity: `Explosion.getIndirectSourceEntity`.
        if (const ServerEntity* source = world.state().entity(sourceEntityId); source != nullptr) {
            const std::optional<std::int32_t> causing = source->ownerId.has_value() ? source->ownerId
                                                        : server::registry::entityAttributes::isLiving(source->type())
                                                            ? std::optional{source->entityId()}
                                                            : std::nullopt;
            entity::entityCombat::blame(*entity, causing, source->entityId(), source->type());
        }
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*entity, damage, x, z, hooks_, "explosion");
        if (!result.landed) {
            continue;
        }
        // parity: `resolvePlayerResponsibleForDamage`, the player behind the blast.
        if (const ServerEntity* source = world.state().entity(sourceEntityId); source != nullptr) {
            rememberShooter(watchers, *source, *entity);
        }
        announceEntityHit(watchers, *entity, world::damageTypes::kExplosion, sourceEntityId, result.died);
        emitVibration(world, "entity_damage", entity->x, entity->y, entity->z, entity->uuid());
    }
}

bool CombatSystem::hitVehicle(world::LoadedWorld& world, ServerEntity* target, float amount, const Identifier& type,
                              const ServerEntity& projectile) {
    if (target == nullptr ||
        (!world::detectorRails::isMinecart(target->type()) && !entity::boats::isBoat(target->type()))) {
        return false;
    }
    if (options_.hurtVehicle) {
        static_cast<void>(options_.hurtVehicle(world, *target,
                                               VehicleSystem::Hit{.amount = amount,
                                                                  .type = type,
                                                                  .causeId = projectile.ownerId,
                                                                  .directId = projectile.entityId(),
                                                                  .burningProjectile = projectile.fireTicks > 0}));
    }
    return true;
}

void CombatSystem::markWitherFriend(world::LoadedWorld& world, ServerEntity& target,
                                    std::optional<std::int32_t> sourceId) {
    if (target.wither == nullptr || !sourceId.has_value()) {
        return;
    }
    // parity: `WitherBoss.hurtServer` asks `source.getEntity()`.
    const ServerEntity* source = world.state().entity(*sourceId);
    if (source != nullptr && source->ownerId.has_value()) {
        source = world.state().entity(*source->ownerId);
    }
    if (source != nullptr && entity::witherBoss::isFriend(source->type().path())) {
        target.wither->nextHitFromFriend = true;
    }
}

Component CombatSystem::shooterName(const std::vector<PlayerSession*>& watchers,
                                    const std::vector<ServerEntity*>& living, const ServerEntity& projectile) {
    if (!projectile.ownerId.has_value()) {
        return entity::entityCombat::displayName(projectile);
    }
    for (const PlayerSession* candidate : watchers) {
        if (candidate->entityId() == *projectile.ownerId) {
            return Component::text(candidate->name());
        }
    }
    for (const ServerEntity* candidate : living) {
        if (candidate->entityId() == *projectile.ownerId) {
            return entity::entityCombat::displayName(*candidate);
        }
    }
    return entity::entityCombat::displayName(projectile);
}

void CombatSystem::announceEntityHit(const std::vector<PlayerSession*>& watchers, ServerEntity& entity,
                                     const Identifier& damageType, std::int32_t causeEntityId, bool died) {
    const std::vector<registry::vanillaRegistryData::Entry>& types = registry::vanillaRegistryData::damageTypes();
    std::int32_t registryId = -1;
    for (std::size_t index = 0; index < types.size(); index++) {
        if (types[index].first == damageType.path()) {
            registryId = static_cast<std::int32_t>(index);
            break;
        }
    }
    const std::string sound = died ? entity::entityCombat::deathSound(entity) : entity::entityCombat::hurtSound(entity);
    // parity: `LivingEntity.makeSound`.
    random::RandomSource voice = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const float volume = entity::mobSounds::volume(entity);
    const float pitch = entity::mobSounds::voicePitch(entity, voice);
    if (!died) {
        entity.ambientSoundTime = -entity::mobSounds::ambientInterval(entity);
    }
    for (PlayerSession* watcher : watchers) {
        // A client on the other edition hears only the hit: flinch and knockback come from entity sync metadata.
        if (watcher->connection().playSound) {
            watcher->connection().playSound(sound, entity.x, entity.y, entity.z, volume, pitch);
            continue;
        }
        if (!watcher->connection().sendPlayPacket) {
            continue;
        }
        if (registryId >= 0) {
            watcher->connection().sendPlayPacket(
                wire::protocol::play::DamageEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::DamageEventPacket{.entityId = entity.entityId(),
                                                            .damageType = registryId,
                                                            .causeEntityId = causeEntityId,
                                                            .directEntityId = causeEntityId}
                        .write(buffer);
                });
        }
        watcher->connection().sendPlayPacket(
            wire::protocol::play::SetEntityMotionPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                wire::protocol::play::SetEntityMotionPacket{.entityId = entity.entityId(),
                                                            .velocityX = entity.velocityX,
                                                            .velocityY = entity.velocityY,
                                                            .velocityZ = entity.velocityZ}
                    .write(buffer);
            });
        watcher->connection().sendPlayPacket(wire::protocol::play::SoundPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            wire::protocol::play::SoundPacket{.sound = sound,
                                              .source = entity::mobSounds::source(entity),
                                              .x = entity.x,
                                              .y = entity.y,
                                              .z = entity.z,
                                              .volume = volume,
                                              .pitch = pitch,
                                              .seed = options_.nextSeed ? options_.nextSeed() : 0}
                .write(buffer);
        });
        if (died) {
            watcher->connection().sendPlayPacket(
                wire::protocol::play::EntityEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                    wire::protocol::play::EntityEventPacket{
                        .entityId = entity.entityId(),
                        .eventId = static_cast<std::int8_t>(wire::protocol::play::EntityEventPacket::kLivingDeath)}
                        .write(buffer);
                });
        }
    }
    if (!died) {
        return;
    }
    entity.killed = true;
    entity.dying = true;
}

} // namespace basalt::server::runtime
