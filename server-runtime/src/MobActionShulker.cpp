#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/player/Combat.hpp>
#include <basalt/server/runtime/player/PlayerConnection.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>
#include <span>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

namespace mth = wire::common::mth;
namespace shulkers = entity::shulkers;

/// parity: `Shulker.COVERED_ARMOR_MODIFIER`, twenty points of armour with the lid shut.
constexpr std::string_view kCoveredModifier = "minecraft:covered";
constexpr double kCoveredArmor = 20.0;
/// parity: `LevelEvent.PARTICLES_SHULKER_TELEPORT`... sent as the teleport's own event.
constexpr std::int32_t kTeleportEvent = 2016;
constexpr double kLevelEventRange = 64.0;
/// parity: `ShulkerBullet.onHitEntity`, four points and ten seconds of levitation.
constexpr float kBulletDamage = 4.0F;
constexpr std::int32_t kLevitationTicks = 200;
/// parity: `EntityTypes.SHULKER_BULLET`'s box.
constexpr double kBulletSize = 0.3125;

[[nodiscard]] std::array<std::int32_t, 3> blockOf(const ServerEntity& entity) {
    return {mth::floor(entity.x), mth::floor(entity.y), mth::floor(entity.z)};
}

/// parity: `BlockUtil.packDifferenceInPosition` over eight blocks each way.
[[nodiscard]] std::int32_t packDifference(const std::array<std::int32_t, 3>& from,
                                          const std::array<std::int32_t, 3>& to) {
    return (((to[0] - from[0] + 8) & 255) << 16) | (((to[1] - from[1] + 8) & 255) << 8) | ((to[2] - from[2] + 8) & 255);
}

} // namespace

void MobActionSystem::shulkerPeek(ServerEntity& shulker, std::int32_t amount) {
    shulkers::ShulkerState& state = shulkers::stateOf(shulker);
    world::LoadedWorld* world = worldOf(shulker);
    // parity: `setRawPeekAmount`.
    // call, even one that does not change the lid.
    auto& armor = shulker.attributeModifiers[std::string{entity::attribute::kArmor}];
    armor.erase(std::string{kCoveredModifier});
    if (amount == 0) {
        armor.insert_or_assign(std::string{kCoveredModifier},
                               entity::AttributeModifier{.id = std::string{kCoveredModifier}, .amount = kCoveredArmor});
    }
    if (world != nullptr) {
        playSound(*world, shulker, amount == 0 ? "entity.shulker.close" : "entity.shulker.open");
        world::vibrations::emit(world->state(), amount == 0 ? "container_close" : "container_open", shulker.x,
                                shulker.y, shulker.z, world->gameTime(), {.entity = shulker.uuid()});
    }
    state.peek = amount;
    shulkers::save(shulker);
}

void MobActionSystem::shootShulkerBullet(ServerEntity& shulker, std::int32_t targetId) {
    world::LoadedWorld* world = worldOf(shulker);
    const Identifier type = Identifier::vanilla("shulker_bullet");
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(type);
    if (world == nullptr || entry == nullptr) {
        return;
    }
    shulkers::ShulkerState& state = shulkers::stateOf(shulker);
    // parity: the bullet leaves from the middle of the shulker's box as it stands.
    const entity::Aabb box =
        shulkers::progressAabb(1.0F, server::registry::opposite(state.attachFace),
                               shulkers::physicalPeek(state.currentPeek), shulker.x, shulker.y, shulker.z);
    const double centreY = (box.minY + box.maxY) * 0.5;
    auto created = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), type,
                                                  entry->protocolId, (box.minX + box.maxX) * 0.5,
                                                  centreY - (kBulletSize / 2.0), (box.minZ + box.maxZ) * 0.5);
    ServerEntity& bullet = world->state().spawnEntity(std::move(created));
    bullet.ownerId = shulker.entityId();
    bullet.noGravity = false;
    bullet.bullet = std::make_shared<shulkers::BulletState>();
    bullet.bullet->target = targetId;
    bullet.bullet->direction = server::registry::BlockFace::Up;
    random::RandomSource random = freshRandom();
    shulkers::selectNextMoveDirection(world->state(), bullet, *bullet.bullet,
                                      static_cast<std::uint8_t>(server::registry::axisOf(state.attachFace)),
                                      bulletTarget(*world, targetId), random);
    const float pitch = ((random.nextFloat() - random.nextFloat()) * 0.2F) + 1.0F;
    playSound(*world, shulker, "entity.shulker.shoot", 2.0F, pitch);
}

std::optional<std::array<double, 4>> MobActionSystem::bulletTarget(world::LoadedWorld& world, std::int32_t targetId) {
    if (const ServerEntity* mob = world.state().entity(targetId); mob != nullptr) {
        if (mob->removed || mob->dying) {
            return std::nullopt;
        }
        return std::array<double, 4>{mob->x, mob->y, mob->z, mob->height()};
    }
    for (const PlayerSession* player : watchersOf(world)) {
        if (player->entityId() == targetId && !player->dead &&
            player->gamemode != wire::protocol::play::Gamemode::Spectator) {
            return std::array<double, 4>{player->x, player->y, player->z, player->sneaking ? 1.5 : 1.8};
        }
    }
    return std::nullopt;
}

void MobActionSystem::tickShulker(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                  const std::vector<ServerEntity*>& living, ServerEntity& shulker,
                                  random::RandomSource& random) {
    shulkers::ShulkerState& state = shulkers::stateOf(shulker);
    const std::span<ServerEntity* const> others{living};
    // parity: what `Shulker.hurtServer` does once a hit lands.
    // in four, and one struck by a bullet with its lid open splits.
    if (shulker.health < state.healthSeen && state.struckBy.has_value()) {
        const std::string struck = *std::exchange(state.struckBy, std::nullopt);
        if (shulker.health < shulker.attribute("minecraft:max_health") * 0.5 && random.nextInt(4) == 0) {
            static_cast<void>(teleportShulker(world, watchers, living, shulker, random));
        } else if (struck == "shulker_bullet") {
            splitShulker(world, watchers, living, shulker, random);
        }
    }
    state.struckBy.reset();
    state.healthSeen = shulker.health;
    if (!shulker.vehicleId.has_value() &&
        !shulkers::canStayAt(world.state(), shulker, others, blockOf(shulker), state.attachFace)) {
        // parity: `findNewAttachment`, another face of the same block or a jump elsewhere.
        if (const std::optional<server::registry::BlockFace> face =
                shulkers::attachableSurface(world.state(), shulker, others, blockOf(shulker));
            face.has_value()) {
            state.attachFace = *face;
            shulkers::save(shulker);
        } else {
            static_cast<void>(teleportShulker(world, watchers, living, shulker, random));
        }
    }
    static_cast<void>(shulkers::updatePeek(state));
}

bool MobActionSystem::teleportShulker(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                      const std::vector<ServerEntity*>& living, ServerEntity& shulker,
                                      random::RandomSource& random) {
    if (shulker.noAi || shulker.dying || shulker.health <= 0.0) {
        return false;
    }
    const std::int32_t minY = config::dimensionFacts(world.declaration().dimension).chunkHeight.minY();
    const std::optional<shulkers::Landing> landing = shulkers::findTeleport(
        world.state(), shulker, std::span<ServerEntity* const>{living}, random, minY, [this](std::int32_t stateId) {
            const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
            return block != nullptr && blockTagged("minecraft:shulker_does_not_teleport_to", *block).value_or(false);
        });
    if (!landing.has_value()) {
        return false;
    }
    const std::array<std::int32_t, 3> from = blockOf(shulker);
    shulkers::ShulkerState& state = shulkers::stateOf(shulker);
    state.attachFace = landing->face;
    playSound(world, shulker, "entity.shulker.teleport");
    // parity: `Shulker.setPos`, which snaps it to the middle of the block it lands in.
    shulker.x = landing->at[0] + 0.5;
    shulker.y = static_cast<double>(landing->at[1]);
    shulker.z = landing->at[2] + 0.5;
    world::vibrations::emit(world.state(), "teleport", from[0] + 0.5, from[1] + 0.5, from[2] + 0.5, world.gameTime(),
                            {.entity = shulker.uuid()});
    const std::int32_t packed = packDifference(from, landing->at);
    for (PlayerSession* player : watchers) {
        const double dx = player->x - from[0];
        const double dy = player->y - from[1];
        const double dz = player->z - from[2];
        if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
            player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(kTeleportEvent, from[0], from[1], from[2], packed);
        }
    }
    state.peek = 0;
    shulker.targetId.reset();
    shulkers::save(shulker);
    return true;
}

void MobActionSystem::splitShulker(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                   const std::vector<ServerEntity*>& living, ServerEntity& shulker,
                                   random::RandomSource& random) {
    const double oldX = shulker.x;
    const double oldY = shulker.y;
    const double oldZ = shulker.z;
    shulkers::ShulkerState& state = shulkers::stateOf(shulker);
    const entity::Aabb oldBox = shulkers::progressAabb(1.0F, server::registry::opposite(state.attachFace),
                                                       shulkers::physicalPeek(state.currentPeek), oldX, oldY, oldZ)
                                    .inflate(8.0);
    // parity: `hitByShulkerBullet` needs the lid open and a teleport that worked.
    if (state.peek == 0 || !teleportShulker(world, watchers, living, shulker, random)) {
        return;
    }
    std::int32_t count = 0;
    for (const ServerEntity* other : living) {
        if (!other->removed && !other->dying && shulkers::isShulker(other->type()) &&
            oldBox.intersects(entity::Aabb::of(other->x, other->y, other->z, 1.0, 1.0))) {
            ++count;
        }
    }
    const float failure = static_cast<float>(count - 1) / 5.0F;
    if (random.nextFloat() < failure) {
        return;
    }
    ServerEntity* baby = spawning_->spawnMob(world, shulker.type(), oldX, oldY, oldZ);
    if (baby != nullptr) {
        shulkers::stateOf(*baby).color = state.color;
        shulkers::save(*baby);
    }
}

void MobActionSystem::tickShulkerBullet(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                        const std::vector<ServerEntity*>& living, ServerEntity& bullet,
                                        random::RandomSource& random) {
    if (bullet.bullet == nullptr) {
        bullet.bullet = std::make_shared<shulkers::BulletState>();
    }
    shulkers::BulletState& state = *bullet.bullet;
    std::optional<std::array<double, 4>> target;
    if (state.target.has_value()) {
        target = bulletTarget(world, *state.target);
        if (!target.has_value()) {
            state.target.reset();
        }
    }
    shulkers::steer(bullet, state, target.has_value());
    // parity: `ProjectileUtil.getHitResultOnMoveVector`.
    const double toX = bullet.x + bullet.velocityX;
    const double toY = bullet.y + bullet.velocityY;
    const double toZ = bullet.z + bullet.velocityZ;
    const std::optional<entity::BlockHit> block =
        entity::blockRay::cast(world.state(), bullet.x, bullet.y, bullet.z, toX, toY, toZ);
    const double endX = block.has_value() ? block->x : toX;
    const double endY = block.has_value() ? block->y : toY;
    const double endZ = block.has_value() ? block->z : toZ;
    ServerEntity* hitMob = nullptr;
    PlayerSession* hitPlayer = nullptr;
    double nearest = 2.0;
    const double grow = kBulletSize / 2.0;
    const auto consider = [&](double x, double y, double z, double width, double height) {
        const double half = (width / 2.0) + grow + 0.3;
        const std::optional<double> along =
            entity::projectiles::clipBox(bullet.x, bullet.y, bullet.z, endX, endY, endZ, x - half, y - 0.3 - grow,
                                         z - half, x + half, y + height + 0.3 + grow, z + half);
        if (along.has_value() && *along < nearest) {
            nearest = *along;
            return true;
        }
        return false;
    };
    for (ServerEntity* candidate : living) {
        if (candidate == &bullet || candidate->removed || candidate->dying ||
            (bullet.ownerId.has_value() && candidate->entityId() == *bullet.ownerId && bullet.tickCount < 2) ||
            shulkers::isBullet(candidate->type())) {
            continue;
        }
        if (consider(candidate->x, candidate->y, candidate->z, candidate->width(), candidate->height())) {
            hitMob = candidate;
            hitPlayer = nullptr;
        }
    }
    for (PlayerSession* player : watchers) {
        if (player->dead || player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        if (consider(player->x, player->y, player->z, 0.6, player->sneaking ? 1.5 : 1.8)) {
            hitPlayer = player;
            hitMob = nullptr;
        }
    }
    bullet.x = toX;
    bullet.y = toY;
    bullet.z = toZ;
    ++bullet.tickCount;
    if (hitMob != nullptr || hitPlayer != nullptr) {
        bulletHitsEntity(world, watchers, bullet, hitMob, hitPlayer);
    } else if (block.has_value()) {
        // parity: `onHitBlock`, two puffs of explosion and the hit sound.
        playSound(world, bullet, "entity.shulker_bullet.hit");
        bullet.removed = true;
        world::vibrations::emit(world.state(), "entity_damage", bullet.x, bullet.y, bullet.z, world.gameTime(),
                                {.entity = bullet.uuid()});
    }
    if (bullet.removed) {
        return;
    }
    // parity: `rotateTowardsMovement(0.5)`, which the client draws the bullet spinning by.
    const double horizontal = std::sqrt((bullet.velocityX * bullet.velocityX) + (bullet.velocityZ * bullet.velocityZ));
    bullet.yaw = static_cast<float>(std::atan2(bullet.velocityX, bullet.velocityZ) * 180.0 / std::numbers::pi);
    bullet.pitch = static_cast<float>(std::atan2(bullet.velocityY, horizontal) * 180.0 / std::numbers::pi);
    if (target.has_value()) {
        shulkers::turnAfterMove(world.state(), bullet, state, *target, random);
    }
}

void MobActionSystem::bulletHitsEntity(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                       ServerEntity& bullet, ServerEntity* mob, PlayerSession* player) {
    bullet.removed = true;
    const ServerEntity* owner = bullet.ownerId.has_value() ? world.state().entity(*bullet.ownerId) : nullptr;
    const world::ActiveEffect levitation{.effect = Identifier::vanilla("levitation"),
                                         .remainingTicks = kLevitationTicks};
    if (mob != nullptr) {
        entity::entityCombat::blame(*mob, bullet.ownerId, bullet.entityId(), bullet.type());
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(*mob, kBulletDamage, bullet.x, bullet.z, hooks_.combat, "mob_projectile");
        if (result.landed) {
            combat_->announceEntityHit(watchers, *mob, Identifier::vanilla("mob_projectile"), bullet.entityId(),
                                       result.died);
            if (!result.died) {
                static_cast<void>(addMobEffect(world, watchers, *mob, levitation,
                                               owner != nullptr ? owner->entityId() : bullet.entityId()));
            }
        }
    }
    if (player != nullptr && player->connection().hurt) {
        player->connection().hurt(player::combat::afterDifficulty(kBulletDamage, world.difficulty()), "mob",
                                  Identifier::vanilla("mob_projectile"),
                                  entity::DamageSource{.entityId = bullet.entityId(),
                                                       .x = bullet.x,
                                                       .z = bullet.z,
                                                       .name = owner != nullptr
                                                                   ? entity::entityCombat::displayName(*owner)
                                                                   : entity::entityCombat::displayName(bullet),
                                                       .ownerId = bullet.ownerId});
        if (player->connection().addEffect && player->gamemode != wire::protocol::play::Gamemode::Creative) {
            player->noteEffectSource(bullet.ownerId.value_or(bullet.entityId()));
            player->connection().addEffect(levitation.effect, kLevitationTicks, 0);
        }
    }
    world::vibrations::emit(world.state(), "entity_damage", bullet.x, bullet.y, bullet.z, world.gameTime(),
                            {.entity = bullet.uuid()});
}

} // namespace basalt::server::runtime
