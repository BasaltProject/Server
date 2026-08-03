#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/AreaEffectCloudSystem.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace basalt::server::runtime {
namespace {

namespace clouds = entity::areaEffectClouds;
using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;

constexpr double kPlayerWidth = 0.6;
constexpr double kPlayerHeight = 1.8;
/// parity: `DragonFireball.onHit`'s own numbers.
constexpr float kFireballCloudRadius = 3.0F;
constexpr float kFireballCloudMaxRadius = 7.0F;
constexpr std::int32_t kFireballCloudDuration = 600;
constexpr double kFireballReach = 16.0;
/// parity: `LevelEvent.PARTICLES_DRAGON_BLOCK_BREAK`'s neighbour.
constexpr std::int32_t kFireballBurstEvent = 2006;
constexpr double kLevelEventRange = 64.0;
const Identifier kIndirectMagic = Identifier::vanilla("indirect_magic");

struct Box {
    double minX;
    double minY;
    double minZ;
    double maxX;
    double maxY;
    double maxZ;
};

[[nodiscard]] bool intersects(const Box& box, double x, double y, double z, double width, double height) {
    const double half = width / 2.0;
    return box.minX < x + half && box.maxX > x - half && box.minY < y + height && box.maxY > y && box.minZ < z + half &&
           box.maxZ > z - half;
}

} // namespace

AreaEffectCloudSystem::AreaEffectCloudSystem(player::PlayerRegistry& players, CombatSystem& combat, Options options)
    : players_(&players), combat_(&combat), options_(options) {}

std::optional<std::int32_t> AreaEffectCloudSystem::spawn(world::LoadedWorld& world, const clouds::Cloud& settings,
                                                         double x, double y, double z,
                                                         std::optional<std::int32_t> owner) {
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(Identifier::vanilla("area_effect_cloud"));
    if (entry == nullptr) {
        return std::nullopt;
    }
    auto cloud = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(), entry->id,
                                                entry->protocolId, x, y, z);
    cloud->noGravity = true;
    cloud->persistent = true;
    cloud->ownerId = owner;
    clouds::write(*cloud, settings);
    return world.state().spawnEntity(std::move(cloud)).entityId();
}

void AreaEffectCloudSystem::tick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 const std::vector<ServerEntity*>& living, ServerEntity& cloud) {
    cloud.tickCount++;
    clouds::Cloud settings = clouds::read(cloud);
    const auto forget = [this, &cloud] {
        cloud.removed = true;
        const std::scoped_lock guard{lock_};
        victims_.erase(cloud.entityId());
    };
    if (settings.duration != -1 && cloud.tickCount - settings.waitTime >= settings.duration) {
        forget();
        return;
    }
    if (cloud.tickCount < settings.waitTime) {
        clouds::write(cloud, settings);
        return;
    }
    float radius = settings.radius;
    if (settings.radiusPerTick != 0.0F) {
        radius += settings.radiusPerTick;
        if (radius < clouds::kMinimalRadius) {
            forget();
            return;
        }
        settings.radius = std::clamp(radius, 0.0F, clouds::kMaxRadius);
    }
    if (cloud.tickCount % clouds::kTimeBetweenApplications != 0) {
        clouds::write(cloud, settings);
        return;
    }
    std::unordered_map<std::int32_t, std::int32_t> touched;
    {
        const std::scoped_lock guard{lock_};
        std::unordered_map<std::int32_t, std::int32_t>& mine = victims_[cloud.entityId()];
        std::erase_if(mine, [&cloud](const auto& entry) { return cloud.tickCount >= entry.second; });
        if (!settings.effect.has_value()) {
            mine.clear();
        }
        touched = mine;
    }
    if (settings.effect.has_value()) {
        const Box box{cloud.x - static_cast<double>(settings.radius), cloud.y,
                      cloud.z - static_cast<double>(settings.radius), cloud.x + static_cast<double>(settings.radius),
                      cloud.y + static_cast<double>(clouds::kHeight), cloud.z + static_cast<double>(settings.radius)};
        const auto inside = [&cloud, radius = settings.radius](double x, double z) {
            const double xd = x - cloud.x;
            const double zd = z - cloud.z;
            return (xd * xd) + (zd * zd) <= static_cast<double>(radius * radius);
        };
        bool gone = false;
        const auto touch = [&](std::int32_t id, PlayerSession* player, ServerEntity* mob, double x, double z) {
            if (gone || touched.contains(id) || !inside(x, z)) {
                return;
            }
            touched[id] = cloud.tickCount + settings.reapplicationDelay;
            affect(world, cloud, settings, player, mob, watchers, living);
            if (settings.radiusOnUse != 0.0F) {
                settings.radius += settings.radiusOnUse;
                if (settings.radius < clouds::kMinimalRadius) {
                    gone = true;
                    return;
                }
            }
            if (settings.durationOnUse != 0 && settings.duration != -1) {
                settings.duration += settings.durationOnUse;
                if (settings.duration <= 0) {
                    gone = true;
                }
            }
        };
        for (PlayerSession* player : watchers) {
            if (!player->dead && intersects(box, player->x, player->y, player->z, kPlayerWidth, kPlayerHeight)) {
                touch(player->entityId(), player, nullptr, player->x, player->z);
            }
        }
        for (ServerEntity* mob : living) {
            if (!mob->removed && !mob->dying && registry::entityAttributes::isLiving(mob->type()) &&
                intersects(box, mob->x, mob->y, mob->z, mob->width(), mob->height())) {
                touch(mob->entityId(), nullptr, mob, mob->x, mob->z);
            }
        }
        if (gone) {
            forget();
            return;
        }
    }
    {
        const std::scoped_lock guard{lock_};
        victims_[cloud.entityId()] = std::move(touched);
    }
    clouds::write(cloud, settings);
}

void AreaEffectCloudSystem::affect(world::LoadedWorld& world, const ServerEntity& cloud, const clouds::Cloud& settings,
                                   PlayerSession* player, ServerEntity* mob,
                                   const std::vector<PlayerSession*>& watchers,
                                   const std::vector<ServerEntity*>& living) {
    const clouds::Effect& effect = *settings.effect;
    const bool harm = effect.id == world::mobEffects::instantDamage();
    const bool heal = effect.id == world::mobEffects::instantHealth();
    if (harm || heal) {
        // parity: the cloud applies an instant effect at half strength, rounded.
        const auto amount = static_cast<std::int32_t>((0.5 * static_cast<double>(6 << effect.amplifier)) + 0.5);
        const auto healed = static_cast<std::int32_t>((0.5 * static_cast<double>(4 << effect.amplifier)) + 0.5);
        const bool undead = mob != nullptr && options_.entityTags != nullptr &&
                            world::realEntityTypeTags::membersOf(*options_.entityTags, Identifier::vanilla("undead"))
                                .contains(mob->type());
        if (harm != undead) {
            if (player != nullptr && player->connection().hurt) {
                player->connection().hurt(
                    static_cast<float>(amount), "indirectMagic", kIndirectMagic,
                    entity::DamageSource{.entityId = cloud.entityId(),
                                         .x = cloud.x,
                                         .z = cloud.z,
                                         .name = CombatSystem::shooterName(watchers, living, cloud)});
            }
            if (mob != nullptr) {
                entity::entityCombat::blame(*mob, cloud.ownerId, cloud.entityId(), cloud.type());
                const entity::entityCombat::Result result = entity::entityCombat::hurt(
                    *mob, static_cast<float>(amount), cloud.x, cloud.z, {}, "indirect_magic");
                if (result.landed) {
                    combat_->announceEntityHit(watchers, *mob, kIndirectMagic, cloud.entityId(), result.died);
                }
            }
        } else {
            if (player != nullptr && player->connection().heal) {
                player->connection().heal(static_cast<float>(healed), "magic");
            }
            if (mob != nullptr) {
                mob->health = std::min(mob->attributeWithModifiers("minecraft:max_health"),
                                       mob->health + static_cast<double>(healed));
            }
        }
        return;
    }
    const auto ticks = static_cast<std::int32_t>(static_cast<float>(effect.duration) * settings.potionDurationScale);
    if (player != nullptr && player->connection().addEffect) {
        player->noteEffectSource(cloud.entityId());
        player->connection().addEffect(effect.id, ticks, effect.amplifier);
    }
    if (mob != nullptr) {
        static_cast<void>(combat_->giveMobEffect(
            world, watchers, *mob,
            world::ActiveEffect{.effect = effect.id, .amplifier = effect.amplifier, .remainingTicks = ticks},
            cloud.entityId()));
    }
}

void AreaEffectCloudSystem::potionLanded(world::LoadedWorld& world, const ServerEntity& bottle,
                                         const std::vector<world::ActiveEffect>& effects) {
    if (effects.empty()) {
        return;
    }
    // parity: `ThrownLingeringPotion.onHitAsPotion`: a three block cloud that thins out over thirty seconds.
    constexpr float kRadius = 3.0F;
    constexpr std::int32_t kDuration = 600;
    clouds::Cloud settings;
    settings.radius = kRadius;
    settings.duration = kDuration;
    settings.waitTime = 10;
    settings.radiusOnUse = -0.5F;
    settings.radiusPerTick = -kRadius / static_cast<float>(kDuration);
    settings.potionDurationScale = 0.25F;
    settings.effect = clouds::Effect{.id = effects.front().effect,
                                     .amplifier = effects.front().amplifier,
                                     .duration = effects.front().remainingTicks};
    static_cast<void>(spawn(world, settings, bottle.x, bottle.y, bottle.z, bottle.ownerId));
}

void AreaEffectCloudSystem::fireballLanded(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                           const std::vector<ServerEntity*>& living, const ServerEntity& fireball) {
    clouds::Cloud settings = clouds::dragonBreath(
        kFireballCloudRadius, kFireballCloudDuration,
        (kFireballCloudMaxRadius - kFireballCloudRadius) / static_cast<float>(kFireballCloudDuration), 1);
    // parity: the living things in the fireball's box grown by four, two and four; the first within.
    // four blocks carries the cloud.
    const Box reach{fireball.x - 4.5, fireball.y - 2.0, fireball.z - 4.5,
                    fireball.x + 4.5, fireball.y + 3.0, fireball.z + 4.5};
    double x = fireball.x;
    double y = fireball.y;
    double z = fireball.z;
    const auto near = [&fireball](double px, double py, double pz) {
        const double dx = px - fireball.x;
        const double dy = py - fireball.y;
        const double dz = pz - fireball.z;
        return (dx * dx) + (dy * dy) + (dz * dz) < kFireballReach;
    };
    bool placed = false;
    for (const PlayerSession* player : watchers) {
        if (!placed && !player->dead &&
            intersects(reach, player->x, player->y, player->z, kPlayerWidth, kPlayerHeight) &&
            near(player->x, player->y, player->z)) {
            x = player->x;
            y = player->y;
            z = player->z;
            placed = true;
        }
    }
    for (const ServerEntity* mob : living) {
        if (!placed && !mob->removed && registry::entityAttributes::isLiving(mob->type()) &&
            intersects(reach, mob->x, mob->y, mob->z, mob->width(), mob->height()) && near(mob->x, mob->y, mob->z)) {
            x = mob->x;
            y = mob->y;
            z = mob->z;
            placed = true;
        }
    }
    const std::int32_t bx = wire::common::mth::floor(fireball.x);
    const std::int32_t by = wire::common::mth::floor(fireball.y);
    const std::int32_t bz = wire::common::mth::floor(fireball.z);
    for (PlayerSession* player : watchers) {
        const double dx = player->x - fireball.x;
        const double dy = player->y - fireball.y;
        const double dz = player->z - fireball.z;
        if ((dx * dx) + (dy * dy) + (dz * dz) <= kLevelEventRange * kLevelEventRange &&
            player->connection().sendLevelEvent) {
            player->connection().sendLevelEvent(kFireballBurstEvent, bx, by, bz, 1);
        }
    }
    // parity: the cloud belongs to whoever threw the fireball.
    static_cast<void>(spawn(world, settings, x, y, z, fireball.ownerId));
}

} // namespace basalt::server::runtime
