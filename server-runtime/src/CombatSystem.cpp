#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/ProjectileHits.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace basalt::server::runtime {
namespace {

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

constexpr double kArrowBaseDamage = 2.0;

/// parity: `AbstractArrow.onHitEntity`'s own `scale(0.2)` when the hit did not land.
constexpr double kArrowBounce = 0.2;

/// parity: `PanicGoal`, which runs for as long as the last hit is remembered.
constexpr std::int32_t kPanicTicks = 40;

/// parity: `death.attack.arrow`, and `AbstractArrow`'s own two points of damage.
constexpr std::string_view kDeathArrow = "arrow";

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
void CombatSystem::emitVibration(world::LoadedWorld& world, std::string_view event, double x, double y, double z) {
    world::vibrations::emit(world.state(), event, x, y, z, world.gameTime());
}

void CombatSystem::tickProjectile(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& entity,
                                  const entity::projectiles::Kind& kind) {
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
        emitVibration(world, "projectile_shoot", entity.x, entity.y, entity.z);
    }
    const std::string_view path = kind.type.path();

    // parity: `AbstractWindCharge.onHitBlock`, whose gust goes off a quarter of a block back along
    // the face it came in through, and `onHitEntity`, which lets it off where it stands.
    if (path == "wind_charge") {
        const std::optional<Target> target = targetAlong(watchers, living, entity, fromX, fromY, fromZ);
        if (!hit.has_value() && !target.has_value()) {
            return;
        }
        entity.removed = true;
        // parity: `AbstractWindCharge.onHitEntity` takes one point off whoever it passed through,
        // and lets the gust off there rather than against a face.
        if (target.has_value()) {
            if (target->mob != nullptr) {
                const entity::entityCombat::Result result =
                    entity::entityCombat::hurt(*target->mob, kWindChargeDamage, fromX, fromZ, hooks_, "wind_charge");
                if (result.landed) {
                    announceEntityHit(watchers, *target->mob, world::damageTypes::kWindCharge, entity.entityId(),
                                      result.died);
                    emitVibration(world, "entity_damage", target->mob->x, target->mob->y, target->mob->z);
                }
            }
            if (target->player != nullptr && target->player->connection().hurt) {
                target->player->connection().hurt(kWindChargeDamage, kDeathWindCharge, world::damageTypes::kWindCharge,
                                                  entity::DamageSource{.entityId = entity.entityId(),
                                                                       .x = fromX,
                                                                       .z = fromZ,
                                                                       .name = shooterName(watchers, living, entity)});
            }
        }
        if (!hit.has_value()) {
            gustWhereItLanded(world, watchers, entity.x, entity.y, entity.z);
            return;
        }
        const double back = world::windBursts::kBackOff;
        gustWhereItLanded(world, watchers, hit->x + (back * server::registry::stepX(hit->face)),
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
            const entity::entityCombat::Result result =
                entity::entityCombat::hurt(*target->mob, kSmallFireballDamage, fromX, fromZ, hooks_, "fireball");
            if (result.landed) {
                announceEntityHit(watchers, *target->mob, world::damageTypes::kOnFire, entity.entityId(), result.died);
                // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit,
                // and only when the damage really came off the health.
                emitVibration(world, "entity_damage", target->mob->x, target->mob->y, target->mob->z);
            }
        }
        if (target->player != nullptr && target->player->connection().hurt) {
            target->player->connection().hurt(kSmallFireballDamage, kDeathOnFire, world::damageTypes::kOnFire,
                                              entity::DamageSource{.entityId = entity.entityId(),
                                                                   .x = fromX,
                                                                   .z = fromZ,
                                                                   .name = shooterName(watchers, living, entity)});
        }
        return;
    }
    if (path == "ender_pearl") {
        if (hit.has_value() || targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            landEnderPearl(watchers, entity);
        }
        return;
    }
    if (path == "potion") {
        if (!hit.has_value() && !targetAlong(watchers, living, entity, fromX, fromY, fromZ).has_value()) {
            return;
        }
        entity.removed = true;
        splashPotion(watchers, entity);
        return;
    }
    if (kind.sticks && hitEntityAlong(world, watchers, living, entity, fromX, fromY, fromZ)) {
        return;
    }
    if (!hit.has_value()) {
        return;
    }
    // parity: `Projectile.onHitBlock` sends `PROJECTILE_LAND` where it landed.
    emitVibration(world, "projectile_land", hit->x, hit->y, hit->z);
    // parity: `Entity.isOnFire`, which is what `TntBlock.onProjectileHit` asks the arrow.
    const std::optional<world::BlockChange> change =
        world::projectileHits::onHit(world.state(), world.gameTime(), *hit, kind.sticks, entity.fireTicks > 0,
                                     world.gameRules().get(world::gameRules::kTntExplodes));
    if (change.has_value()) {
        broadcaster_.announceRedstone(world, watchers, *change);
    }
    if (!kind.sticks) {
        entity.removed = true;
    }
}

void CombatSystem::gustWhereItLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers, double x,
                                     double y, double z) {
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const world::explosions::Spares stops = [this](world::WorldState&, std::int32_t, std::int32_t, std::int32_t,
                                                   std::int32_t stateId) {
        const Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return options_.blockTags != nullptr && block != nullptr &&
               world::realBlockTags::contains(*options_.blockTags, "minecraft:blocks_wind_charge_explosions", *block);
    };
    const std::vector<world::BlockChange> knocked = world::windBursts::burst(
        world.state(), facts.chunkHeight.minY(), facts.chunkHeight.maxY(), x, y, z, world.gameTime(), random, stops);
    for (const world::BlockChange& change : knocked) {
        broadcaster_.announceRedstone(world, watchers, change);
        // parity: `updateNeighbours` tells the wall a button or a lever is stuck to, which is what
        // carries the signal into whatever is behind it.
        if (const std::optional<std::array<std::int32_t, 3>> wall =
                world::blockInteractions::attachmentOf(change.stateId, change.x, change.y, change.z);
            wall.has_value()) {
            broadcaster_.announceRedstone(
                world, watchers,
                world::BlockChange{.x = (*wall)[0],
                                   .y = (*wall)[1],
                                   .z = (*wall)[2],
                                   .stateId =
                                       world.state().blockState((*wall)[0], (*wall)[1], (*wall)[2]).value_or(0)});
        }
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

void CombatSystem::splashPotion(const std::vector<PlayerSession*>& watchers, const ServerEntity& bottle) {
    if (!bottle.potionEffect.has_value()) {
        return;
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
    if (owner->connection().hurt) {
        owner->connection().hurt(kEnderPearlDamage, kDeathFall, world::damageTypes::kEnderPearl, std::nullopt);
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
            (projectile.ownerId.has_value() && candidate->entityId() == *projectile.ownerId)) {
            continue;
        }
        if (!server::registry::entityAttributes::isLiving(candidate->type())) {
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
        if ((projectile.ownerId.has_value() && candidate->entityId() == *projectile.ownerId) || candidate->dead) {
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

bool CombatSystem::hitEntityAlong(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& arrow, double fromX,
                                  double fromY, double fromZ) {
    const std::optional<Target> target = targetAlong(watchers, living, arrow, fromX, fromY, fromZ);
    if (!target.has_value()) {
        return false;
    }
    // parity: `AbstractArrow.onHitEntity`: the damage is the speed times the arrow's own two points,
    // rounded up, and a fully drawn bow adds a roll on top of it.
    const double speed = std::sqrt((arrow.velocityX * arrow.velocityX) + (arrow.velocityY * arrow.velocityY) +
                                   (arrow.velocityZ * arrow.velocityZ));
    auto damage = static_cast<std::int32_t>(std::ceil(speed * kArrowBaseDamage));
    if (arrow.critical && options_.nextInt) {
        damage += options_.nextInt((damage / 2) + 2);
    }

    if (target->mob != nullptr) {
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*target->mob, static_cast<float>(damage), fromX, fromZ, hooks_, "projectile");
        if (!result.landed) {
            // parity: an arrow that found somebody still inside their hurt cooldown bounces off.
            arrow.velocityX *= kArrowBounce;
            arrow.velocityY *= kArrowBounce;
            arrow.velocityZ *= kArrowBounce;
            return true;
        }
        emitVibration(world, "entity_damage", target->mob->x, target->mob->y, target->mob->z);
        arrow.removed = true;
        if (arrow.ownerId.has_value()) {
            target->mob->hurtBy = *arrow.ownerId;
        }
        target->mob->panicTicks = kPanicTicks;
        announceEntityHit(watchers, *target->mob, world::damageTypes::kArrow, arrow.entityId(), result.died);
        return true;
    }
    arrow.removed = true;
    if (target->player != nullptr && target->player->connection().hurt) {
        target->player->connection().hurt(
            static_cast<float>(damage), kDeathArrow, world::damageTypes::kArrow,
            entity::DamageSource{
                .entityId = arrow.entityId(), .x = fromX, .z = fromZ, .name = shooterName(watchers, living, arrow)});
    }
    return true;
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
    const std::string sound =
        died ? entity::entityCombat::deathSound(entity.type()) : entity::entityCombat::hurtSound(entity.type());
    for (PlayerSession* watcher : watchers) {
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
                                              .source = wire::protocol::play::SoundSource::Hostile,
                                              .x = entity.x,
                                              .y = entity.y,
                                              .z = entity.z,
                                              .volume = 1.0F,
                                              .pitch = 1.0F,
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
