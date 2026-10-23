#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/FireworkSystem.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <numbers>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using entity::fireworkRockets::RocketState;
using player::PlayerSession;

/// parity: `horizontalCollision ? 1.0 : 1.15` and the 0.04 climb a free rocket adds every tick.
constexpr double kHorizontalBoost = 1.15;
constexpr double kClimb = 0.04;
/// parity: `ProjectileUtil.getHitResultOnMoveVector`'s 0.3 of margin round a target's box.
constexpr double kHitMargin = 0.3;
/// parity: `dealExplosionDamage`'s five blocks and five plus two a star.
constexpr double kDamageRadius = 5.0;
constexpr float kBaseDamage = 5.0F;
constexpr float kDamagePerStar = 2.0F;
/// parity: `broadcastEntityEvent(this, 17)`, which the client draws the stars from.
constexpr std::int8_t kExplodeEvent = 17;
constexpr float kLaunchVolume = 3.0F;
/// parity: how far a sound of that volume carries, sixteen blocks a unit.
constexpr double kSoundReachPerVolume = 16.0;
/// parity: `Projectile.lerpRotation`'s fifth of the way a tick.
constexpr float kRotationLerp = 0.2F;
constexpr double kDegrees = 180.0 / std::numbers::pi;
/// parity: `EntityType.PLAYER`'s standing box.
constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;

struct Hit {
    ServerEntity* mob = nullptr;
    PlayerSession* player = nullptr;
    std::optional<entity::BlockHit> block;
};

[[nodiscard]] std::optional<double> along(double fromX, double fromY, double fromZ, double toX, double toY, double toZ,
                                          double x, double y, double z, double width, double height) {
    const double half = (width / 2.0) + kHitMargin;
    return entity::projectiles::clipBox(fromX, fromY, fromZ, toX, toY, toZ, x - half, y - kHitMargin, z - half,
                                        x + half, y + height + kHitMargin, z + half);
}

/// parity: `ProjectileUtil.getHitResultOnMoveVector`.
[[nodiscard]] Hit hitAlong(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                           const std::vector<ServerEntity*>& living, const ServerEntity& rocket) {
    Hit hit;
    double toX = rocket.x + rocket.velocityX;
    double toY = rocket.y + rocket.velocityY;
    double toZ = rocket.z + rocket.velocityZ;
    hit.block = entity::blockRay::cast(world.state(), rocket.x, rocket.y, rocket.z, toX, toY, toZ);
    if (hit.block.has_value()) {
        toX = hit.block->x;
        toY = hit.block->y;
        toZ = hit.block->z;
    }
    const RocketState& state = *rocket.firework;
    const auto skip = [&](std::int32_t id) {
        return id == rocket.entityId() || (state.attachedTo == id) ||
               (!state.leftOwner && rocket.ownerId == std::optional{id});
    };
    double nearest = 2.0;
    for (ServerEntity* mob : living) {
        if (mob->removed || mob->health <= 0.0 || skip(mob->entityId())) {
            continue;
        }
        const std::optional<double> at =
            along(rocket.x, rocket.y, rocket.z, toX, toY, toZ, mob->x, mob->y, mob->z, mob->width(), mob->height());
        if (at.has_value() && *at < nearest) {
            nearest = *at;
            hit.mob = mob;
        }
    }
    for (PlayerSession* player : watchers) {
        if (player->dead || player->gamemode == wire::protocol::play::Gamemode::Spectator || skip(player->entityId())) {
            continue;
        }
        const std::optional<double> at = along(rocket.x, rocket.y, rocket.z, toX, toY, toZ, player->x, player->y,
                                               player->z, kPlayerWidth, kPlayerHeight);
        if (at.has_value() && *at < nearest) {
            nearest = *at;
            hit.mob = nullptr;
            hit.player = player;
        }
    }
    return hit;
}

/// parity: `Projectile.checkLeftOwner`.
[[nodiscard]] bool leftOwner(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                             const ServerEntity& rocket) {
    if (!rocket.ownerId.has_value()) {
        return true;
    }
    std::optional<entity::Aabb> owner;
    if (const ServerEntity* mob = world.state().entity(*rocket.ownerId); mob != nullptr) {
        owner = entity::Aabb::of(mob->x, mob->y, mob->z, mob->width(), mob->height());
    }
    for (const PlayerSession* player : watchers) {
        if (player->entityId() == *rocket.ownerId) {
            owner = entity::Aabb::of(player->x, player->y, player->z, kPlayerWidth, kPlayerHeight);
        }
    }
    if (!owner.has_value()) {
        return true;
    }
    const entity::Aabb box = entity::Aabb::of(rocket.x, rocket.y, rocket.z, rocket.width(), rocket.height());
    const entity::Aabb swept = box.expandTowards(rocket.velocityX, rocket.velocityY, rocket.velocityZ).inflate(1.0);
    return !swept.intersects(*owner);
}

void broadcastEvent(const std::vector<PlayerSession*>& watchers, std::int32_t entityId, std::int8_t event) {
    const wire::protocol::play::EntityEventPacket packet{.entityId = entityId, .eventId = event};
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::EntityEventPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

/// parity: `Projectile.updateRotation`, eased a fifth of the way to where it is going.
void updateRotation(ServerEntity& rocket) {
    const double flat = std::sqrt((rocket.velocityX * rocket.velocityX) + (rocket.velocityZ * rocket.velocityZ));
    const auto pitch = static_cast<float>(std::atan2(rocket.velocityY, flat) * kDegrees);
    const auto yaw = static_cast<float>(std::atan2(rocket.velocityX, rocket.velocityZ) * kDegrees);
    const auto lerp = [](float from, float to) {
        while (to - from < -180.0F) {
            from -= 360.0F;
        }
        while (to - from >= 180.0F) {
            from += 360.0F;
        }
        return from + ((to - from) * kRotationLerp);
    };
    rocket.pitch = lerp(rocket.pitch, pitch);
    rocket.yaw = lerp(rocket.yaw, yaw);
}

} // namespace

FireworkSystem::FireworkSystem(player::PlayerRegistry& players, CombatSystem& combat, Options options)
    : players_(&players), combat_(&combat), options_(std::move(options)) {}

ServerEntity* FireworkSystem::launch(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y,
                                     double z, std::optional<std::int32_t> ownerId, bool shotAtAngle) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(entity::fireworkRockets::type());
    if (entry == nullptr) {
        return nullptr;
    }
    auto rocket = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(),
                                                 entity::fireworkRockets::type(), entry->protocolId, x, y, z);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    entity::fireworkRockets::start(*rocket, std::move(item), random).shotAtAngle = shotAtAngle;
    rocket->ownerId = ownerId;
    return &world.state().spawnEntity(std::move(rocket));
}

ServerEntity* FireworkSystem::shoot(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x, double y,
                                    double z, double dirX, double dirY, double dirZ, float power, float uncertainty,
                                    std::optional<std::int32_t> ownerId) {
    ServerEntity* rocket = launch(world, std::move(item), x, y, z, ownerId, true);
    if (rocket != nullptr) {
        random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
        entity::projectiles::shoot(*rocket, dirX, dirY, dirZ, power, uncertainty, random);
    }
    return rocket;
}

ServerEntity* FireworkSystem::attach(world::LoadedWorld& world, wire::nbt::NbtCompound item, std::int32_t entityId,
                                     double x, double y, double z) {
    ServerEntity* rocket = launch(world, std::move(item), x, y, z, entityId);
    if (rocket != nullptr) {
        rocket->firework->attachedTo = entityId;
    }
    return rocket;
}

void FireworkSystem::tick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                          const std::vector<ServerEntity*>& living, ServerEntity& rocket) {
    if (rocket.firework == nullptr) {
        rocket.firework = std::make_shared<RocketState>();
        rocket.firework->item = entity::fireworkRockets::plainRocket();
    }
    RocketState& state = *rocket.firework;
    if (!state.leftOwner) {
        state.leftOwner = leftOwner(world, watchers, rocket);
    }
    Hit hit;
    if (state.attachedTo.has_value()) {
        if (const ServerEntity* mob = world.state().entity(*state.attachedTo); mob != nullptr) {
            rocket.x = mob->x;
            rocket.y = mob->y;
            rocket.z = mob->z;
            rocket.velocityX = mob->velocityX;
            rocket.velocityY = mob->velocityY;
            rocket.velocityZ = mob->velocityZ;
        }
        for (const PlayerSession* player : watchers) {
            if (player->entityId() == *state.attachedTo) {
                rocket.x = player->x;
                rocket.y = player->y;
                rocket.z = player->z;
            }
        }
        hit = hitAlong(world, watchers, living, rocket);
    } else {
        if (!state.shotAtAngle) {
            const double boost = rocket.horizontallyBlocked ? 1.0 : kHorizontalBoost;
            rocket.velocityX *= boost;
            rocket.velocityY += kClimb;
            rocket.velocityZ *= boost;
        }
        const double wantX = rocket.velocityX;
        const double wantY = rocket.velocityY;
        const double wantZ = rocket.velocityZ;
        hit = hitAlong(world, watchers, living, rocket);
        // parity: `move(MoverType.SELF, movement)` then the movement put back as it was.
        const entity::Aabb box = entity::Aabb::of(rocket.x, rocket.y, rocket.z, rocket.width(), rocket.height());
        const std::array<double, 3> moved = entity::entityCollision::collide(
            [&world](std::int32_t x, std::int32_t y, std::int32_t z) { return world.state().blockState(x, y, z); }, box,
            wantX, wantY, wantZ);
        rocket.x += moved[0];
        rocket.y += moved[1];
        rocket.z += moved[2];
        rocket.horizontallyBlocked = moved[0] != wantX || moved[2] != wantZ;
        rocket.onGround = moved[1] != wantY && wantY < 0.0;
    }
    if (!rocket.removed) {
        if (hit.mob != nullptr || hit.player != nullptr) {
            explode(world, watchers, living, rocket);
            return;
        }
        if (hit.block.has_value() && entity::fireworkRockets::explosionCount(state.item) > 0) {
            explode(world, watchers, living, rocket);
            return;
        }
    }
    updateRotation(rocket);
    const bool silent = rocket.sourceNbt.has_value() && rocket.sourceNbt->getBoolean("Silent");
    if (state.life == 0 && !silent) {
        const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
        const double reach = kSoundReachPerVolume * static_cast<double>(kLaunchVolume);
        for (const PlayerSession* player : watchers) {
            const double dx = player->x - rocket.x;
            const double dy = player->y - rocket.y;
            const double dz = player->z - rocket.z;
            if ((dx * dx) + (dy * dy) + (dz * dz) < reach * reach) {
                player::playSoundFor(*player, "entity.firework_rocket.launch",
                                     wire::protocol::play::SoundSource::Ambient, rocket.x, rocket.y, rocket.z,
                                     kLaunchVolume, 1.0F, seed);
            }
        }
    }
    if (++state.life > state.lifetime) {
        explode(world, watchers, living, rocket);
    }
}

void FireworkSystem::explode(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                             const std::vector<ServerEntity*>& living, ServerEntity& rocket) {
    if (!world.events().allowed(
            &server::plugins::pluginEventTypes::entityFireworkExplode,
            [&](server::plugins::PluginEvent& event) { plugins::hookEvents::entity(event, world.name(), rocket); })) {
        rocket.removed = true;
        return;
    }
    broadcastEvent(watchers, rocket.entityId(), kExplodeEvent);
    std::optional<wire::common::Uuid> owner;
    if (rocket.ownerId.has_value()) {
        if (const ServerEntity* mob = world.state().entity(*rocket.ownerId); mob != nullptr) {
            owner = mob->uuid();
        }
        for (const PlayerSession* player : watchers) {
            if (player->entityId() == *rocket.ownerId) {
                owner = player->uuid();
            }
        }
    }
    world::vibrations::emit(world.state(), "explode", rocket.x, rocket.y, rocket.z, world.gameTime(),
                            {.entity = owner});
    dealExplosionDamage(world, watchers, living, rocket);
    rocket.removed = true;
}

void FireworkSystem::dealExplosionDamage(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                         const std::vector<ServerEntity*>& living, ServerEntity& rocket) {
    const std::int32_t stars = entity::fireworkRockets::explosionCount(rocket.firework->item);
    if (stars == 0) {
        return;
    }
    const float amount = kBaseDamage + (static_cast<float>(stars) * kDamagePerStar);
    const std::optional<std::int32_t> attached = rocket.firework->attachedTo;
    const auto hurtPlayer = [&](PlayerSession& player, float damage) {
        if (player.connection().hurt) {
            player.connection().hurt(damage, "fireworks", world::damageTypes::kFireworks,
                                     entity::DamageSource{.entityId = rocket.entityId(),
                                                          .x = rocket.x,
                                                          .z = rocket.z,
                                                          .name = CombatSystem::shooterName(watchers, living, rocket),
                                                          .ownerId = rocket.ownerId});
        }
    };
    const auto hurtMob = [&](ServerEntity& mob, float damage) {
        entity::entityCombat::blame(mob, rocket.ownerId, rocket.entityId(), rocket.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, damage, rocket.x, rocket.z, {}, "fireworks");
        if (result.landed) {
            combat_->announceEntityHit(watchers, mob, world::damageTypes::kFireworks, rocket.entityId(), result.died);
        }
    };
    if (attached.has_value()) {
        if (ServerEntity* mob = world.state().entity(*attached); mob != nullptr) {
            hurtMob(*mob, amount);
        }
        for (PlayerSession* player : watchers) {
            if (player->entityId() == *attached) {
                hurtPlayer(*player, amount);
            }
        }
    }
    const entity::Aabb reach =
        entity::Aabb::of(rocket.x, rocket.y, rocket.z, rocket.width(), rocket.height()).inflate(kDamageRadius);
    // parity: a clear line to the feet or to the middle of the body, and less the further off.
    const auto damageAt = [&](double x, double y, double z, double width, double height) -> std::optional<float> {
        if (!reach.intersects(entity::Aabb::of(x, y, z, width, height))) {
            return std::nullopt;
        }
        const double dx = x - rocket.x;
        const double dy = y - rocket.y;
        const double dz = z - rocket.z;
        const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        if (distanceSquared > kDamageRadius * kDamageRadius) {
            return std::nullopt;
        }
        bool seen = false;
        for (std::int32_t step = 0; step < 2 && !seen; ++step) {
            seen = !entity::blockRay::cast(world.state(), rocket.x, rocket.y, rocket.z, x, y + (height * 0.5 * step), z)
                        .has_value();
        }
        if (!seen) {
            return std::nullopt;
        }
        return amount * static_cast<float>(std::sqrt((kDamageRadius - std::sqrt(distanceSquared)) / kDamageRadius));
    };
    for (ServerEntity* mob : living) {
        if (mob->removed || mob->entityId() == attached) {
            continue;
        }
        if (const std::optional<float> damage = damageAt(mob->x, mob->y, mob->z, mob->width(), mob->height());
            damage.has_value()) {
            hurtMob(*mob, *damage);
        }
    }
    for (PlayerSession* player : watchers) {
        if (player->dead || player->entityId() == attached) {
            continue;
        }
        if (const std::optional<float> damage = damageAt(player->x, player->y, player->z, kPlayerWidth, kPlayerHeight);
            damage.has_value()) {
            hurtPlayer(*player, *damage);
        }
    }
}

} // namespace basalt::server::runtime
