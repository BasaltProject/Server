#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExperienceOrbSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/MobSounds.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/WardenBrain.hpp>
#include <basalt/server/runtime/entity/WitherBoss.hpp>
#include <basalt/server/runtime/player/PlayerDamage.hpp>
#include <basalt/server/runtime/player/PlayerImpulse.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
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

/// parity: `LevelEvent.PARTICLES_SPELL_POTION_SPLASH`, heard within sixty-four blocks, with the
/// colour an experience bottle's splash is drawn in.
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

/// parity: `AbstractWindCharge.onHitEntity`'s single point, and the death message key of
/// `DamageTypes.WIND_CHARGE`.
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

/// parity: `EntityType.PLAYER`, whose box is what a projectile's reach is measured against.
const Identifier& playerType() {
    static const Identifier kType = Identifier::vanilla("player");
    return kType;
}

/// Where along its flight the projectile met this box, or nothing when it did not.
std::optional<double> clip(const ServerEntity& arrow, double fromX, double fromY, double fromZ, double x, double y,
                           double z, double width, double height) {
    return entity::projectiles::clipBox(fromX, fromY, fromZ, arrow.x, arrow.y, arrow.z, x - (width / 2.0), y,
                                        z - (width / 2.0), x + (width / 2.0), y + height, z + (width / 2.0));
}

} // namespace

CombatSystem::CombatSystem(BlockChangeBroadcaster& broadcaster, ExplosionSystem& explosions, Hooks hooks,
                           Options options)
    : broadcaster_(broadcaster), explosions_(explosions), hooks_(std::move(hooks)), options_(std::move(options)) {}

/// parity: `Level.gameEvent`, which is what a sculk sensor is listening to. Nobody to tell here: the
/// noise still has to travel, and the sensor lights up on its own turn.
void CombatSystem::emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z,
                                 const wire::common::Uuid& source) {
    world::vibrations::emit(world.state(), event, x, y, z, world.gameTime(), {.entity = source});
}

void CombatSystem::tickProjectile(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& entity,
                                  const entity::projectiles::Kind& kind) {
    const bool trident = kind.type.path() == "trident";
    if (trident && tridentReturns(world, watchers, living, entity)) {
        return;
    }
    const double fromX = entity.x;
    const double fromY = entity.y;
    const double fromZ = entity.z;
    const std::optional<entity::BlockHit> hit = entity::projectiles::tick(world.state(), entity, kind);
    // parity: `Projectile.tick` announces the shot on the first turn, and a sculk sensor hears it at
    // frequency three whether anybody fired it or not. It goes **after** the move because every
    // projectile tick (`AbstractArrow`, `ThrowableProjectile`, `AbstractHurtingProjectile`) moves
    // first and calls `super.tick()` last, so the shot is announced from where it got to. Measured
    // in `flechavibra`: the jar reads power eight, which is four squares, and from the square it
    // appeared in it would be five.
    if (!entity.announced) {
        entity.announced = true;
        emitVibration(world, "projectile_shoot", entity.x, entity.y, entity.z, entity.uuid());
    }
    const std::string_view path = kind.type.path();
    if (deflected(watchers, living, entity, fromX, fromY, fromZ)) {
        return;
    }

    // parity: `AbstractWindCharge.onHitBlock`, whose gust goes off a quarter of a block back along
    // the face it came in through, and `onHitEntity`, which lets it off where it stands.
    if (path == "wind_charge" || path == "breeze_wind_charge") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        // parity: `AbstractWindCharge.onHitEntity` takes one point off whoever it passed through,
        // and lets the gust off there rather than against a face.
        if (target.has_value()) {
            if (target->mob != nullptr && world::endCrystals::isCrystal(target->mob->type())) {
                hitCrystal(world, watchers, *target->mob, false);
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
        const double back = world::windBursts::kBackOff;
        gustWhereItLanded(world, watchers, living, entity, hit->x + (back * server::registry::stepX(hit->face)),
                          hit->y + (back * server::registry::stepY(hit->face)),
                          hit->z + (back * server::registry::stepZ(hit->face)));
        return;
    }

    // parity: `AbstractHurtingProjectile.onHit`: a fireball goes off where it lands, whether that is
    // a block or somebody's face.
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
            explosions_.blowUp(world, watchers, entity.x, entity.y, entity.z, entity.explosionPower, "tnt",
                               entity.entityId());
            return;
        }
        if (hit.has_value()) {
            lightWhereItLanded(world, watchers, *hit);
        }
        if (!target.has_value()) {
            return;
        }
        // parity: a small fireball burns whoever it hits for five seconds and takes five off.
        if (target->mob != nullptr) {
            target->mob->fireTicks = kSmallFireballFireTicks;
            entity::entityCombat::blame(*target->mob, entity.ownerId, entity.entityId(), entity.type());
            const entity::entityCombat::Result result =
                entity::entityCombat::hurt(*target->mob, kSmallFireballDamage, fromX, fromZ, hooks_, "fireball");
            if (result.landed) {
                announceEntityHit(watchers, *target->mob, world::damageTypes::kOnFire, entity.entityId(), result.died);
                // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit,
                // and only when the damage really came off the health.
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
    // parity: `DragonFireball.onHit`, which bursts on anything but its own dragon, and whose tick
    // throws it away once the dragon is gone.
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
        const std::int32_t amount = 3 + (options_.nextInt ? options_.nextInt(5) + options_.nextInt(5) : 0);
        if (options_.awardExperience) {
            options_.awardExperience(world, entity.x, entity.y, entity.z, amount);
        }
        return;
    }
    if (path == "potion") {
        if (!hit.has_value() && !targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
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
        return;
    }
    // parity: `AbstractArrow.onHitBlock` sounds, forgets its piercing once it sticks, and keeps the
    // plain arrow's sound after.
    if (trident) {
        channelOnBlock(world, watchers, entity, *hit);
    }
    if (kind.sticks) {
        arrowHitSound(watchers, entity);
        entity.arrowLanded = true;
        entity.pierceLevel = 0;
        entity.piercedIds.clear();
    }
    // parity: `Projectile.onHitBlock` sends `PROJECTILE_LAND` where it landed.
    emitVibration(world, "projectile_land", hit->x, hit->y, hit->z, entity.uuid());
    // parity: `Entity.isOnFire`, which is what `TntBlock.onProjectileHit` asks the arrow.
    const std::optional<world::BlockChange> change = world::projectileHits::onHit(
        world.state(), world.gameTime(), *hit, kind.sticks, entity.fireTicks > 0,
        world.gameRules().get(world::gameRules::kTntExplodes),
        [this, &world](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
            broadcaster_.dropBrokenBlock(world, bx, by, bz, stateId, true);
        });
    if (change.has_value()) {
        broadcaster_.announceRedstone(world, watchers, *change);
        // parity: `TargetBlock.onProjectileHit`, for a projectile a player owns.
        const Identifier* struck = server::registry::blockStateDefinitions::blockOf(change->stateId);
        if (struck != nullptr && struck->path() == "target" && entity.ownerId.has_value() && options_.targetHit) {
            for (PlayerSession* shooter : watchers) {
                if (shooter->entityId() == *entity.ownerId) {
                    const std::string power =
                        server::registry::blockStateDefinitions::propertiesOf(change->stateId)["power"];
                    options_.targetHit(world, *shooter, entity, power.empty() ? 0 : std::stoi(power));
                    break;
                }
            }
        }
    }
    if (!kind.sticks) {
        entity.removed = true;
    }
}

void CombatSystem::lightWhereItLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                      const entity::BlockHit& hit) {
    // parity: `SmallFireball.onHitBlock` lights the square on the face it came in through, and with
    // no owner there is no `mobGriefing` to ask.
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
        const double share = 1.0 - (std::sqrt(distanceSquared) / kSplashRange);
        const auto ticks = static_cast<std::int32_t>(kSplashDuration * share);
        if (ticks > 0) {
            static_cast<void>(giveMobEffect(
                world, watchers, *mob,
                world::ActiveEffect{.effect = Identifier::vanilla(*bottle.potionEffect), .remainingTicks = ticks},
                bottle.entityId()));
        }
    }
    for (PlayerSession* player : watchers) {
        const double dx = player->x - bottle.x;
        const double dy = player->y - bottle.y;
        const double dz = player->z - bottle.z;
        const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        if (distanceSquared > kSplashRange * kSplashRange) {
            continue;
        }
        // parity: `ThrownSplashPotion.applySplash`, where the further away you are the less of the
        // effect you get, down to nothing at the edge.
        const double share = 1.0 - (std::sqrt(distanceSquared) / kSplashRange);
        const auto ticks = static_cast<std::int32_t>(kSplashDuration * share);
        if (ticks <= 0 || !player->connection().addEffect) {
            continue;
        }
        player->connection().addEffect(Identifier::vanilla(*bottle.potionEffect), ticks, 0);
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
    // parity: `hitTargetOrDeflectSelf`: the deflector keeps it from landing, and turns it back only
    // the first time in a row.
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
    // parity: `ProjectileDeflection.REVERSE` at full power: back at half the speed, turned a little
    // over half a turn either way.
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
                                                              double fromY, double fromZ) {
    double nearest = std::numeric_limits<double>::max();
    Target found;
    for (ServerEntity* candidate : living) {
        if (candidate->entityId() == projectile.entityId() ||
            (projectile.ownerId.has_value() && candidate->entityId() == *projectile.ownerId) ||
            std::ranges::contains(projectile.piercedIds, candidate->entityId())) {
            continue;
        }
        // parity: `Projectile.canHitEntity` asks `isPickable`, which a crystal is too.
        if (!server::registry::entityAttributes::isLiving(candidate->type()) &&
            !world::endCrystals::isCrystal(candidate->type())) {
            continue;
        }
        // parity: `EnderDragon.isPickable` is false; what a projectile meets is one of its parts.
        if (candidate->dragon != nullptr) {
            for (std::size_t part = 0; part < entity::enderDragon::kParts; ++part) {
                const entity::enderDragon::Vec3& centre = candidate->dragon->parts[part];
                const entity::enderDragon::PartBox& box = entity::enderDragon::kPartBoxes[part];
                const std::optional<double> at = clip(projectile, fromX, fromY, fromZ, centre.x, centre.y, centre.z,
                                                      static_cast<double>(box.width), static_cast<double>(box.height));
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
                                              candidate->width(), candidate->height());
        if (!at.has_value() || *at >= nearest) {
            continue;
        }
        nearest = *at;
        found = Target{.mob = candidate, .player = nullptr};
    }
    for (PlayerSession* candidate : watchers) {
        if ((projectile.ownerId.has_value() && candidate->entityId() == *projectile.ownerId) || candidate->dead ||
            std::ranges::contains(projectile.piercedIds, candidate->entityId())) {
            continue;
        }
        if (candidate->gamemode == Gamemode::Creative || candidate->gamemode == Gamemode::Spectator) {
            continue;
        }
        const std::optional<double> at = clip(projectile, fromX, fromY, fromZ, candidate->x, candidate->y, candidate->z,
                                              server::registry::entityTypeShapes::widthOf(playerType()),
                                              server::registry::entityTypeShapes::heightOf(playerType()));
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
        if (!server::registry::entityAttributes::isLiving(entity->type())) {
            continue;
        }
        const float damage = world::explosions::damageAt(distance, power, static_cast<float>(reach));
        if (damage <= 0.0F) {
            continue;
        }
        markWitherFriend(world, *entity, sourceEntityId);
        // parity: `Explosion.getIndirectSourceEntity`: a projectile's or a primed block's owner, or a
        // living source itself.
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

void CombatSystem::markWitherFriend(world::LoadedWorld& world, ServerEntity& target,
                                    std::optional<std::int32_t> sourceId) {
    if (target.wither == nullptr || !sourceId.has_value()) {
        return;
    }
    // parity: `WitherBoss.hurtServer` asks `source.getEntity()`, the owner behind a skull or arrow.
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
    // parity: `LivingEntity.makeSound` at the mob's own volume, voice and category, and
    // `Mob.playHurtSound`, which puts its next ambient sound off.
    random::RandomSource voice = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const float volume = entity::mobSounds::volume(entity);
    const float pitch = entity::mobSounds::voicePitch(entity, voice);
    if (!died) {
        entity.ambientSoundTime = -entity::mobSounds::ambientInterval(entity);
    }
    for (PlayerSession* watcher : watchers) {
        // A client on the other edition hears the hit and nothing else of this: the flinch and the
        // knock back are drawn from its own metadata, which the entity sync already carries.
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
