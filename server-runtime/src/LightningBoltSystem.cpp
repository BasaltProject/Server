#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
namespace mth = wire::common::mth;

/// parity: the box a bolt reaches, three blocks round and six more above.
constexpr double kReach = 3.0;
constexpr double kHeight = 6.0;
/// parity: `Entity.thunderHit`.
constexpr float kLightningDamage = 5.0F;
/// parity: `spawnFire(4)` on the strike.
constexpr std::int32_t kStrikeFires = 4;
/// parity: `GameRules.FIRE_SPREAD_RADIUS_AROUND_PLAYER`.
constexpr world::GameRule kFireSpreadRadius = world::GameRule::integer("fire_spread_radius_around_player", 128, -1);

/// parity: `ServerLevel.canSpreadFireAround`: the rule off, or somebody not spectating near enough.
[[nodiscard]] bool canSpreadFireAround(const world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                       std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t radius = world.gameRules().number(kFireSpreadRadius);
    if (radius == -1) {
        return true;
    }
    for (const PlayerSession* player : watchers) {
        if (player->gamemode == wire::protocol::play::Gamemode::Spectator) {
            continue;
        }
        const double dx = player->x - x;
        const double dy = player->y - y;
        const double dz = player->z - z;
        if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) < static_cast<double>(radius)) {
            return true;
        }
    }
    return false;
}

} // namespace

LightningBoltSystem::LightningBoltSystem(BlockChangeBroadcaster& broadcaster, MobActionSystem& mobs)
    : broadcaster_(&broadcaster), mobs_(&mobs) {}

void LightningBoltSystem::tick(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                               const std::vector<ServerEntity*>& living, ServerEntity& bolt,
                               random::RandomSource& random) {
    // parity: the constructor's roll of one to three flashes.
    if (bolt.boltFlashes < 0) {
        bolt.boltFlashes = random.nextInt(3) + 1;
    }
    const bool fierce = world.difficulty() == "normal" || world.difficulty() == "hard";
    std::vector<world::BlockChange> changes;
    // parity: `life == 2` does everything to the blocks: the rod it hit answers, the ground catches
    // on normal and hard, and it is heard as fourteen.
    if (bolt.boltLife == 2) {
        // parity: `getStrikePosition`, which nudges down by a millionth so a bolt standing on a
        // whole number hits the block under its feet and not the air it is in.
        world::lightning::strike(world.state(), mth::floor(bolt.x), mth::floor(bolt.y - 1.0e-6), mth::floor(bolt.z),
                                 false, random, changes);
        if (fierce &&
            canSpreadFireAround(world, watchers, mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z))) {
            world::lightning::spawnFire(world.state(), mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z),
                                        kStrikeFires, random, changes);
        }
        world::vibrations::emit(world.state(), "lightning_strike", bolt.x, bolt.y, bolt.z, world.gameTime(),
                                {.entity = bolt.uuid()});
    }
    bolt.boltLife--;
    if (bolt.boltLife < 0) {
        if (bolt.boltFlashes == 0) {
            bolt.removed = true;
        } else if (bolt.boltLife < -random.nextInt(10)) {
            bolt.boltFlashes--;
            bolt.boltLife = 1;
            if (canSpreadFireAround(world, watchers, mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z))) {
                world::lightning::spawnFire(world.state(), mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z),
                                            0, random, changes);
            }
        }
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
    if (bolt.boltLife >= 0) {
        hitAround(world, watchers, living, bolt);
    }
}

void LightningBoltSystem::hitAround(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                    const std::vector<ServerEntity*>& living, const ServerEntity& bolt) {
    const auto touches = [&bolt](double x, double y, double z, double halfWidth, double height) {
        return x + halfWidth > bolt.x - kReach && x - halfWidth < bolt.x + kReach && y + height > bolt.y - kReach &&
               y < bolt.y + kHeight + kReach && z + halfWidth > bolt.z - kReach && z - halfWidth < bolt.z + kReach;
    };
    for (ServerEntity* entity : living) {
        if (entity == &bolt || entity->removed || entity->dying || entity->health <= 0.0 ||
            entity->type() == world::lightning::bolt() ||
            !touches(entity->x, entity->y, entity->z, entity->width() / 2.0, entity->height())) {
            continue;
        }
        mobs_->thunderHit(world, watchers, *entity, bolt.entityId());
    }
    // parity: `Entity.thunderHit` on an item: five points against its five health, and gone.
    for (const world::DroppedItem& item : world.state().nearbyItems(bolt.x, bolt.y, bolt.z, 2.0 * (kReach + kHeight))) {
        if (!touches(item.x, item.y, item.z, 0.125, 0.25)) {
            continue;
        }
        static_cast<void>(world.state().hurtItem(item.entityId, static_cast<std::int32_t>(kLightningDamage)));
        world::vibrations::emit(world.state(), "entity_damage", item.x, item.y, item.z, world.gameTime(),
                                {.entity = item.uuid});
    }
    for (PlayerSession* player : watchers) {
        if (player->dead || !touches(player->x, player->y, player->z, 0.3, 1.8)) {
            continue;
        }
        // parity: a player rests at minus twenty or zero, never minus one, so the tick is all it gets.
        player->fireTicks++;
        if (player->connection().hurt) {
            player->connection().hurt(kLightningDamage, "lightningBolt", world::damageTypes::kLightningBolt,
                                      std::nullopt);
        }
    }
}

} // namespace basalt::server::runtime
