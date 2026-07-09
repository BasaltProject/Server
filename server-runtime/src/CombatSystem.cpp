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
namespace {

/// parity: `LevelEvent.PARTICLES_SPELL_POTION_SPLASH`.
constexpr std::int32_t kSplashPotionEvent = 2002;
constexpr std::int32_t kWaterBottleColour = -13083194;
constexpr double kLevelEventRange = 64.0;

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::Gamemode;
using wire::text::Component;

/// parity: `SmallFireball.onHitEntity`, five points and five seconds of burning.
constexpr float kSmallFireballDamage = 5.0F;
constexpr std::int32_t kSmallFireballFireTicks = 100;
constexpr std::string_view kDeathOnFire = "onFire";

/// parity: `AbstractWindCharge.onHitEntity`.
constexpr float kWindChargeDamage = 1.0F;
constexpr std::string_view kDeathWindCharge = "mob";

/// parity: `applySplash`, four blocks across and twenty seconds at the centre.
constexpr double kSplashRange = 4.0;
constexpr double kSplashDuration = 400.0;
constexpr std::string_view kSoundSplash = "entity.splash_potion.break";

constexpr std::string_view kSoundTeleport = "entity.player.teleport";
constexpr std::string_view kDeathFall = "fall";

/// parity: `ThrownEnderpearl.onHit`, five points of fall damage on arrival.
constexpr float kEnderPearlDamage = 5.0F;

/// parity: `EntityType.PLAYER`.
const Identifier& playerType() {
    static const Identifier kType = Identifier::vanilla("player");
    return kType;
}

std::optional<double> clip(const ServerEntity& arrow, double fromX, double fromY, double fromZ, double x, double y,
                           double z, double width, double height, double margin) {
    const double half = (width / 2.0) + margin;
    return entity::projectiles::clipBox(fromX, fromY, fromZ, arrow.x, arrow.y, arrow.z, x - half, y - margin, z - half,
                                        x + half, y + height + margin, z + half);
}

} // namespace

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

void CombatSystem::landEnderPearl(const std::vector<PlayerSession*>& watchers, ServerEntity& pearl) {
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
    if (owner->connection().teleportTo) {
        owner->connection().teleportTo(pearl.x, pearl.y, pearl.z, owner->yaw, owner->pitch);
    }
    // parity: `ThrownEnderpearl.onHit` lets go of the thrower's impulse.
    player::impulse::reset(*owner);
    if (owner->connection().hurt) {
        owner->connection().hurt(kEnderPearlDamage, kDeathFall, world::damageTypes::kEnderPearl, std::nullopt);
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
        explosions_.blowUp(world, watchers, crystal.x, crystal.y, crystal.z, world::endCrystals::kExplosionPower,
                           "end_crystal", crystal.entityId());
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
