#include "CombatSystemShared.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
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

CombatSystem::CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks,
                           Options options)
    : broadcaster_(broadcaster), explosions_(explosions), hooks_(std::move(hooks)), options_(std::move(options)) {}

/// parity: `Level.gameEvent`.
void CombatSystem::emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                                 const wire::common::Uuid& source) {
    world::vibrations::emit(world.state(), event, x, y, z, world.gameTime(), {.entity = source});
}

/// parity: `Level.gameEvent`.
void CombatSystem::emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                                 const world::vibrations::Source& source) {
    world::vibrations::emit(world.state(), event, x, y, z, world.gameTime(), source);
}

/// parity: `Projectile.getOwner`, resolved to the uuid the NBT keeps under `Owner`.
[[nodiscard]] std::optional<wire::common::Uuid> projectileOwnerOf(world::LoadedWorld& world,
                                                                  const std::vector<PlayerSession*>& watchers,
                                                                  const ServerEntity& projectile) {
    if (projectile.ownerUuid.has_value()) {
        return projectile.ownerUuid;
    }
    if (!projectile.ownerId.has_value()) {
        return std::nullopt;
    }
    for (const PlayerSession* player : watchers) {
        if (player->entityId() == *projectile.ownerId) {
            return player->uuid();
        }
    }
    for (const ServerEntity* other : world.state().entities()) {
        if (other->entityId() == *projectile.ownerId) {
            return other->uuid();
        }
    }
    return std::nullopt;
}

void CombatSystem::tickProjectile(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& entity,
                                  const entity::projectiles::Kind& kind) {
    if (kind.type.path() == "llama_spit") {
        tickLlamaSpit(world, watchers, living, entity, kind);
        return;
    }
    const bool trident = kind.type.path() == "trident";
    if (trident && tridentReturns(world, watchers, living, entity)) {
        return;
    }

    struct ForcedHit {
        ServerEntity& projectile;

        ~ForcedHit() {
            projectile.forcedHitId.reset();
            projectile.forcedLand = false;
        }
    } const forcedHit{entity};

    const double fromX = entity.x;
    const double fromY = entity.y;
    const double fromZ = entity.z;
    const std::optional<entity::BlockHit> hit = entity::projectiles::tick(world.state(), entity, kind);
    // parity: `Projectile.tick` answers the shoot with the owner as the source entity.
    if (!entity.announced) {
        entity.announced = true;
        emitVibration(world, "projectile_shoot", entity.x, entity.y, entity.z,
                      {.entity = projectileOwnerOf(world, watchers, entity)});
    }
    const std::string_view path = kind.type.path();
    if (deflected(watchers, living, entity, fromX, fromY, fromZ)) {
        return;
    }

    if (!kind.sticks) {
        const std::optional<Target> struck = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (hit.has_value() || struck.has_value()) {
            const std::int32_t struckId =
                struck.has_value() ? (struck->mob != nullptr ? struck->mob->entityId() : struck->player->entityId())
                                   : -1;
            const auto bx = static_cast<std::int32_t>(std::floor(hit.has_value() ? hit->x : entity.x));
            const auto by = static_cast<std::int32_t>(std::floor(hit.has_value() ? hit->y : entity.y));
            const auto bz = static_cast<std::int32_t>(std::floor(hit.has_value() ? hit->z : entity.z));
            if (!hitAllowed(world, entity, struckId, bx, by, bz)) {
                return;
            }
            if (entity.bounces) {
                bounceOff(entity, struck.has_value() ? std::nullopt : hit);
                return;
            }
        }
    }
    // parity: `AbstractWindCharge.onHitBlock`.
    if (path == "wind_charge" || path == "breeze_wind_charge") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!entity.forcedLand && !hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        // parity: `AbstractWindCharge.onHitEntity`.
        if (target.has_value()) {
            if (target->mob != nullptr && world::endCrystals::isCrystal(target->mob->type())) {
                hitCrystal(world, watchers, *target->mob, false);
            } else if (hitVehicle(world, target->mob, kWindChargeDamage, world::damageTypes::kWindCharge, entity)) {
                // parity: `AbstractWindCharge.onHitEntity` hurts whatever it met, a vehicle included.
            } else if (target->mob != nullptr && !breezeProof(living, entity, *target->mob)) {
                if (target->mob->dragon != nullptr) {
                    target->mob->dragon->hitPart = target->dragonPart;
                }
                entity::entityCombat::blame(*target->mob, entity.ownerId, entity.entityId(), entity.type());
                const entity::entityCombat::Result result =
                    entity::entityCombat::hurt(*target->mob, kWindChargeDamage, fromX, fromZ, hooks_, "wind_charge",
                                               std::nullopt, firedByPlayer(watchers, entity));
                if (result.landed) {
                    rememberShooter(watchers, entity, *target->mob);
                    announceEntityHit(watchers, *target->mob, world::damageTypes::kWindCharge, entity.entityId(),
                                      result.died);
                    emitVibration(world, "entity_damage", target->mob->x, target->mob->y, target->mob->z,
                                  target->mob->uuid());
                }
            }
            if (target->player != nullptr && target->player->connection().hurt) {
                target->player->connection().hurt(kWindChargeDamage, kDeathWindCharge, world::damageTypes::kWindCharge,
                                                  entity::DamageSource{.entityId = entity.entityId(),
                                                                       .x = fromX,
                                                                       .z = fromZ,
                                                                       .name = shooterName(watchers, living, entity),
                                                                       .ownerId = entity.ownerId});
            }
        }
        if (!hit.has_value()) {
            gustWhereItLanded(world, watchers, living, entity, entity.x, entity.y, entity.z);
            return;
        }
        if (entity.incendiary.value_or(false)) {
            lightWhereItLanded(world, watchers, *hit);
        }
        const double back = world::windBursts::kBackOff;
        gustWhereItLanded(world, watchers, living, entity, hit->x + (back * server::registry::stepX(hit->face)),
                          hit->y + (back * server::registry::stepY(hit->face)),
                          hit->z + (back * server::registry::stepZ(hit->face)));
        return;
    }

    // parity: `AbstractHurtingProjectile.onHit`.
    if (path == "fireball" || path == "small_fireball") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        if (path == "fireball") {
            if (target.has_value()) {
                hitByLargeFireball(world, watchers, living, entity, *target, fromX, fromZ);
            }
            // parity: `LargeFireball.onHit` sets fire wherever it explodes when mobs may grief.
            explosions_.blowUp(world, watchers, entity.x, entity.y, entity.z, entity.explosionPower, "tnt",
                               entity.entityId(), {}, {},
                               entity.incendiary.value_or(world.gameRules().get(world::gameRules::kMobGriefing)));
            return;
        }
        if (hit.has_value() && entity.incendiary.value_or(true)) {
            lightWhereItLanded(world, watchers, *hit);
        }
        if (!target.has_value()) {
            return;
        }
        // parity: a small fireball burns whoever it hits for five seconds and takes five off.
        if (hitVehicle(world, target->mob, kSmallFireballDamage,
                       Identifier::vanilla(entity.ownerId.has_value() ? "fireball" : "unattributed_fireball"),
                       entity)) {
            return;
        }
        if (target->mob != nullptr) {
            target->mob->fireTicks = kSmallFireballFireTicks;
            entity::entityCombat::blame(*target->mob, entity.ownerId, entity.entityId(), entity.type());
            const entity::entityCombat::Result result =
                entity::entityCombat::hurt(*target->mob, kSmallFireballDamage, fromX, fromZ, hooks_, "fireball");
            if (result.landed) {
                announceEntityHit(watchers, *target->mob, world::damageTypes::kOnFire, entity.entityId(), result.died);
                // parity: `LivingEntity.hurtServer`.
                emitVibration(world, "entity_damage", target->mob->x, target->mob->y, target->mob->z,
                              target->mob->uuid());
            }
        }
        if (target->player != nullptr && target->player->connection().hurt) {
            target->player->connection().hurt(kSmallFireballDamage, kDeathOnFire, world::damageTypes::kOnFire,
                                              entity::DamageSource{.entityId = entity.entityId(),
                                                                   .x = fromX,
                                                                   .z = fromZ,
                                                                   .name = shooterName(watchers, living, entity),
                                                                   .ownerId = entity.ownerId});
        }
        return;
    }
    // parity: `DragonFireball.onHit`.
    if (path == "dragon_fireball") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        if (options_.dragonFireballLanded) {
            options_.dragonFireballLanded(world, watchers, living, entity);
        }
        return;
    }
    if (path == "wither_skull") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        // parity: `WitherSkull.onHitEntity` hurts a vehicle like anything else before the blast.
        if (target.has_value() && target->mob != nullptr &&
            (world::detectorRails::isMinecart(target->mob->type()) || entity::boats::isBoat(target->mob->type()))) {
            const ServerEntity* owner = entity.ownerId.has_value() ? world.state().entity(*entity.ownerId) : nullptr;
            const bool ownedByLiving =
                owner != nullptr && !owner->removed && server::registry::entityAttributes::isLiving(owner->type());
            static_cast<void>(hitVehicle(world, target->mob, ownedByLiving ? 8.0F : 5.0F,
                                         ownedByLiving ? world::damageTypes::kWitherSkull : world::damageTypes::kMagic,
                                         entity));
            if (options_.witherSkullLanded) {
                options_.witherSkullLanded(world, watchers, entity, nullptr, nullptr);
            }
            return;
        }
        if (options_.witherSkullLanded) {
            options_.witherSkullLanded(world, watchers, entity, target.has_value() ? target->mob : nullptr,
                                       target.has_value() ? target->player : nullptr);
        }
        return;
    }
    if (path == "ender_pearl") {
        if (hit.has_value() || targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            landEnderPearl(watchers, entity);
        }
        return;
    }
    // parity: `ThrownExperienceBottle.onHit`: the splash, and three to eleven points as orbs.
    if (path == "experience_bottle") {
        if (!hit.has_value() && !targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            return;
        }
        entity.removed = true;
        const std::int32_t bx = static_cast<std::int32_t>(std::floor(entity.x));
        const std::int32_t by = static_cast<std::int32_t>(std::floor(entity.y));
        const std::int32_t bz = static_cast<std::int32_t>(std::floor(entity.z));
        for (PlayerSession* player : watchers) {
            const double dx = player->x - entity.x;
            const double dy = player->y - entity.y;
            const double dz = player->z - entity.z;
            if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
                player->connection().sendLevelEvent) {
                player->connection().sendLevelEvent(kSplashPotionEvent, bx, by, bz, kWaterBottleColour);
            }
        }
        std::int32_t amount = 3 + (options_.nextInt ? options_.nextInt(5) + options_.nextInt(5) : 0);
        if (const server::plugins::PluginEvent* answered =
                world.events().ask(&server::plugins::pluginEventTypes::entityExpBottle,
                                   [&](server::plugins::PluginEvent& event) {
                                       plugins::hookEvents::entity(event, world.name(), entity);
                                       event.set("experience", static_cast<std::int64_t>(amount));
                                       event.set("show_effect", true);
                                   });
            answered != nullptr) {
            if (const auto* edited = std::get_if<std::int64_t>(&answered->get("experience")); edited != nullptr) {
                amount = static_cast<std::int32_t>(std::max<std::int64_t>(0, *edited));
            }
        }
        if (options_.awardExperience) {
            options_.awardExperience(world, entity.x, entity.y, entity.z, amount);
        }
        return;
    }
    if (path == "potion") {
        if (!entity.forcedLand && !hit.has_value() &&
            !targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            return;
        }
        entity.removed = true;
        splashPotion(world, watchers, living, entity);
        return;
    }
    // parity: `AbstractArrow.stepMoveAndHit`, which keeps going through whatever it pierces.
    if (kind.sticks) {
        ArrowHit struck = ArrowHit::Missed;
        do {
            struck = trident ? tridentHitEntity(world, watchers, living, entity, fromX, fromY, fromZ)
                             : hitEntityAlong(world, watchers, living, entity, fromX, fromY, fromZ);
        } while (struck == ArrowHit::Pierced);
        if (struck == ArrowHit::Stopped) {
            return;
        }
    }
    if (!hit.has_value()) {
        // parity: `Projectile.onHit` discards a thrown projectile that struck anything.
        if (!kind.sticks && targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            entity.removed = true;
        }
        return;
    }
    if (!hitAllowed(world, entity, -1, static_cast<std::int32_t>(std::floor(hit->x)),
                    static_cast<std::int32_t>(std::floor(hit->y)), static_cast<std::int32_t>(std::floor(hit->z)))) {
        return;
    }
    if (entity.bounces) {
        bounceOff(entity, hit);
        return;
    }
    // parity: `AbstractArrow.onHitBlock`.
    if (trident) {
        channelOnBlock(world, watchers, entity, *hit);
    }
    if (kind.sticks) {
        arrowHitSound(watchers, entity);
        entity.arrowLanded = true;
        entity.pierceLevel = 0;
        entity.piercedIds.clear();
    }
    hitBlock(world, watchers, entity, *hit, kind.sticks);
    if (!kind.sticks) {
        entity.removed = true;
    }
}

void CombatSystem::bounceOff(ServerEntity& projectile, const std::optional<entity::BlockHit>& block) {
    constexpr double kBounceKept = 0.8;
    if (!block.has_value()) {
        projectile.velocityX = -projectile.velocityX * kBounceKept;
        projectile.velocityY = -projectile.velocityY * kBounceKept;
        projectile.velocityZ = -projectile.velocityZ * kBounceKept;
        return;
    }
    const double stepX = server::registry::stepX(block->face);
    const double stepY = server::registry::stepY(block->face);
    const double stepZ = server::registry::stepZ(block->face);
    const double into =
        (projectile.velocityX * stepX) + (projectile.velocityY * stepY) + (projectile.velocityZ * stepZ);
    projectile.velocityX = (projectile.velocityX - (2.0 * into * stepX)) * kBounceKept;
    projectile.velocityY = (projectile.velocityY - (2.0 * into * stepY)) * kBounceKept;
    projectile.velocityZ = (projectile.velocityZ - (2.0 * into * stepZ)) * kBounceKept;
}

bool CombatSystem::hitAllowed(world::LoadedWorld& world, const ServerEntity& projectile, std::int32_t hitId,
                              std::int32_t x, std::int32_t y, std::int32_t z) const {
    return !options_.projectileHit || options_.projectileHit(world, projectile, hitId, x, y, z);
}

void CombatSystem::hitBlock(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                            const ServerEntity& projectile, const entity::BlockHit& hit, bool byArrow) {
    // parity: `Projectile.onHitBlock` sends `PROJECTILE_LAND` where it landed, owning player included.
    emitVibration(world, "projectile_land", hit.x, hit.y, hit.z,
                  {.entity = projectile.uuid(), .projectileOwner = projectileOwnerOf(world, watchers, projectile)});
    // parity: `Entity.isOnFire`, which is what `TntBlock.onProjectileHit` asks the arrow.
    const std::optional<world::BlockChange> change = world::projectileHits::onHit(
        world.state(), world.gameTime(), hit, byArrow, projectile.fireTicks > 0,
        world.gameRules().get(world::gameRules::kTntExplodes),
        [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            broadcaster_.dropBrokenBlock(world, bx, by, bz, stateId, true);
        });
    if (!change.has_value()) {
        return;
    }
    broadcaster_.announceRedstone(world, watchers, *change);
    // parity: `TargetBlock.onProjectileHit`, for a projectile a player owns.
    const Identifier* struck = server::registry::blockStateDefinitions::blockOf(change->stateId);
    if (struck != nullptr && struck->path() == "target" && projectile.ownerId.has_value() && options_.targetHit) {
        for (PlayerSession* shooter : watchers) {
            if (shooter->entityId() == *projectile.ownerId) {
                const std::string power =
                    server::registry::blockStateDefinitions::propertiesOf(change->stateId)["power"];
                options_.targetHit(world, *shooter, projectile, power.empty() ? 0 : std::stoi(power));
                break;
            }
        }
    }
}

void CombatSystem::lightWhereItLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                      const entity::BlockHit& hit) {
    // parity: `SmallFireball.onHitBlock`.
    const std::int32_t x = hit.blockX + server::registry::stepX(hit.face);
    const std::int32_t y = hit.blockY + server::registry::stepY(hit.face);
    const std::int32_t z = hit.blockZ + server::registry::stepZ(hit.face);
    const std::int32_t delay = world::fires::kMinDelay + options_.nextInt(world::fires::kDelaySpread);
    if (!world::fires::igniteOnImpact(world.state(), x, y, z, delay)) {
        return;
    }
    const std::optional<std::int32_t> lit = world.state().blockState(x, y, z);
    if (lit.has_value()) {
        broadcaster_.announceRedstone(world, watchers, world::BlockChange{x, y, z, *lit, {}});
    }
}

bool CombatSystem::giveMobEffect(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& mob, const world::ActiveEffect& effect, std::int32_t sourceId) const {
    return options_.mobEffect && options_.mobEffect(world, watchers, mob, effect, sourceId);
}

void CombatSystem::splashPotion(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                const std::vector<ServerEntity*>& living, const ServerEntity& bottle) {
    if (!bottle.potionEffect.has_value()) {
        return;
    }

    struct Splashed {
        ServerEntity* mob = nullptr;
        PlayerSession* player = nullptr;
        double share = 0.0;
    };

    std::vector<Splashed> splashed;
    // parity: `applySplash` reaches every living thing in range, mobs as much as players.
    for (ServerEntity* mob : living) {
        if (mob->removed || mob->dying || !server::registry::entityAttributes::isLiving(mob->type())) {
            continue;
        }
        const double dx = mob->x - bottle.x;
        const double dy = mob->y - bottle.y;
        const double dz = mob->z - bottle.z;
        const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        if (distanceSquared > kSplashRange * kSplashRange) {
            continue;
        }
        splashed.push_back({mob, nullptr, 1.0 - (std::sqrt(distanceSquared) / kSplashRange)});
    }
    for (PlayerSession* player : watchers) {
        const double dx = player->x - bottle.x;
        const double dy = player->y - bottle.y;
        const double dz = player->z - bottle.z;
        const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        if (distanceSquared > kSplashRange * kSplashRange) {
            continue;
        }
        splashed.push_back({nullptr, player, 1.0 - (std::sqrt(distanceSquared) / kSplashRange)});
    }
    const auto idOf = [](const Splashed& hit) {
        return hit.mob != nullptr ? hit.mob->entityId() : hit.player->entityId();
    };
    const server::plugins::PluginEvent* answered = world.events().ask(
        &server::plugins::pluginEventTypes::entityPotionSplash, [&](server::plugins::PluginEvent& event) {
            std::string lines;
            for (const Splashed& hit : splashed) {
                lines += (lines.empty() ? "" : "\n") + std::to_string(idOf(hit)) + "|" + std::to_string(hit.share);
            }
            plugins::hookEvents::entity(event, world.name(), bottle);
            event.set("effect", *bottle.potionEffect);
            event.set("affected", std::move(lines));
        });
    if (answered != nullptr && answered->cancelled()) {
        splashed.clear();
    } else if (const auto* edited =
                   answered == nullptr ? nullptr : std::get_if<std::string>(&answered->get("affected"));
               edited != nullptr) {
        std::vector<Splashed> kept;
        std::size_t from = 0;
        while (from < edited->size()) {
            const std::size_t end = std::min(edited->find('\n', from), edited->size());
            const std::string line = edited->substr(from, end - from);
            from = end + 1;
            const std::size_t bar = line.find('|');
            if (bar == std::string::npos) {
                continue;
            }
            const auto id = static_cast<std::int32_t>(std::strtol(line.c_str(), nullptr, 10));
            const auto found = std::ranges::find_if(splashed, [&](const Splashed& hit) { return idOf(hit) == id; });
            if (found != splashed.end()) {
                kept.push_back({found->mob, found->player, std::strtod(line.c_str() + bar + 1, nullptr)});
            }
        }
        splashed = std::move(kept);
    }
    for (const Splashed& hit : splashed) {
        const auto ticks = static_cast<std::int32_t>(kSplashDuration * hit.share);
        if (ticks <= 0) {
            continue;
        }
        if (hit.mob != nullptr) {
            static_cast<void>(giveMobEffect(
                world, watchers, *hit.mob,
                world::ActiveEffect{.effect = Identifier::vanilla(*bottle.potionEffect), .remainingTicks = ticks},
                bottle.entityId()));
        } else if (hit.player->connection().addEffect) {
            // parity: `getEffectSource`, the thrower or else the bottle.
            hit.player->noteEffectSource(bottle.ownerId.value_or(bottle.entityId()));
            hit.player->connection().addEffect(Identifier::vanilla(*bottle.potionEffect), ticks, 0);
        }
    }
    broadcaster_.sendEffects(watchers, static_cast<std::int32_t>(std::floor(bottle.x)),
                             static_cast<std::int32_t>(std::floor(bottle.y)),
                             static_cast<std::int32_t>(std::floor(bottle.z)), server::registry::blocks::air().stateId,
                             {world::blockEffect::Sound{.name = std::string{kSoundSplash},
                                                        .source = wire::protocol::play::SoundSource::Neutral,
                                                        .volume = 1.0F,
                                                        .pitch = 1.0F}});
}

} // namespace basalt::server::runtime
