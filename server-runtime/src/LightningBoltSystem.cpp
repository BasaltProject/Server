#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

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
/// parity: the box `LightningBolt.tick` looks for bystanders in, and who hears of it.
constexpr double kViewerReach = 15.0;
constexpr double kAnnounceDistance = 256.0;
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

LightningBoltSystem::LightningBoltSystem(BlockChangeBroadcaster& broadcaster, MobActionSystem& mobs,
                                         world::AdvancementTriggers* triggers)
    : broadcaster_(&broadcaster), mobs_(&mobs), triggers_(triggers) {}

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
        if (fierce && !bolt.boltVisualOnly &&
            canSpreadFireAround(world, watchers, mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z))) {
            const std::size_t before = changes.size();
            world::lightning::spawnFire(world.state(), mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z),
                                        kStrikeFires, random, changes);
            bolt.boltFiresSet += static_cast<std::int32_t>(changes.size() - before);
        }
        world::vibrations::emit(world.state(), "lightning_strike", bolt.x, bolt.y, bolt.z, world.gameTime(),
                                {.entity = bolt.uuid()});
    }
    bolt.boltLife--;
    if (bolt.boltLife < 0) {
        if (bolt.boltFlashes == 0) {
            announceStrike(world, watchers, living, bolt);
            bolt.removed = true;
        } else if (bolt.boltLife < -random.nextInt(10)) {
            bolt.boltFlashes--;
            bolt.boltLife = 1;
            if (!bolt.boltVisualOnly &&
                canSpreadFireAround(world, watchers, mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z))) {
                const std::size_t before = changes.size();
                world::lightning::spawnFire(world.state(), mth::floor(bolt.x), mth::floor(bolt.y), mth::floor(bolt.z),
                                            0, random, changes);
                bolt.boltFiresSet += static_cast<std::int32_t>(changes.size() - before);
            }
        }
    }
    for (const world::BlockChange& change : changes) {
        broadcaster_->announceRedstone(world, watchers, change);
    }
    if (bolt.boltLife >= 0 && !bolt.boltVisualOnly) {
        hitAround(world, watchers, living, bolt);
    }
}

void LightningBoltSystem::hitAround(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                    const std::vector<ServerEntity*>& living, ServerEntity& bolt) {
    const auto touches = [&bolt](double x, double y, double z, double halfWidth, double height) {
        return x + halfWidth > bolt.x - kReach && x - halfWidth < bolt.x + kReach && y + height > bolt.y - kReach &&
               y < bolt.y + kHeight + kReach && z + halfWidth > bolt.z - kReach && z - halfWidth < bolt.z + kReach;
    };
    // parity: `CHANNELED_LIGHTNING` for the bolt's cause, every turn, with whatever this turn touched.
    world::TriggerEvent channeled;
    for (ServerEntity* entity : living) {
        if (entity == &bolt || entity->removed || entity->dying || entity->health <= 0.0 ||
            entity->type() == world::lightning::bolt() ||
            !touches(entity->x, entity->y, entity->z, entity->width() / 2.0, entity->height())) {
            continue;
        }
        mobs_->thunderHit(world, watchers, *entity, bolt.entityId());
        bolt.boltHitIds.push_back(entity->entityId());
        channeled.victims.push_back(world::triggerFacts::ofEntity(world, *entity));
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
        bolt.boltHitIds.push_back(player->entityId());
        // parity: a player rests at minus twenty or zero, never minus one, so the tick is all it gets.
        player->fireTicks++;
        if (player->connection().hurt) {
            player->connection().hurt(kLightningDamage, "lightningBolt", world::damageTypes::kLightningBolt,
                                      std::nullopt);
        }
        channeled.victims.push_back(world::triggerFacts::ofPlayer(*player));
    }
    if (triggers_ == nullptr || !bolt.boltCause.has_value()) {
        return;
    }
    for (PlayerSession* cause : watchers) {
        if (cause->entityId() == *bolt.boltCause) {
            triggers_->fire("channeled_lightning", cause->name(), cause->uuid().toString(), channeled);
        }
    }
}

ServerEntity* LightningBoltSystem::spawn(world::LoadedWorld& world, double x, double y, double z, std::int32_t entityId,
                                         std::optional<std::int32_t> cause, bool visualOnly) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(world::lightning::bolt());
    if (entry == nullptr) {
        return nullptr;
    }
    auto bolt = std::make_unique<ServerEntity>(entityId, wire::common::Uuid::random(), world::lightning::bolt(),
                                               entry->protocolId, x, y, z);
    bolt->boltCause = cause;
    bolt->boltVisualOnly = visualOnly;
    return &world.state().spawnEntity(std::move(bolt));
}

void LightningBoltSystem::announceStrike(const world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                         const std::vector<ServerEntity*>& living, const ServerEntity& bolt) const {
    if (triggers_ == nullptr || !triggers_->listensTo("lightning_strike")) {
        return;
    }
    const auto hit = [&bolt](std::int32_t id) { return std::ranges::contains(bolt.boltHitIds, id); };
    // parity: the entities in a box fifteen out and twenty-one up that the bolt did not hit.
    const auto sees = [&bolt](double x, double y, double z) {
        return x >= bolt.x - kViewerReach && x <= bolt.x + kViewerReach && y >= bolt.y - kViewerReach &&
               y <= bolt.y + kHeight + kViewerReach && z >= bolt.z - kViewerReach && z <= bolt.z + kViewerReach;
    };
    std::vector<::basalt::server::world::loot::EntityFacts> viewers;
    for (const ServerEntity* entity : living) {
        if (entity != &bolt && !entity->removed && !entity->dying && sees(entity->x, entity->y, entity->z) &&
            !hit(entity->entityId())) {
            viewers.push_back(world::triggerFacts::ofEntity(world, *entity));
        }
    }
    for (const PlayerSession* player : watchers) {
        if (!player->dead && sees(player->x, player->y, player->z) && !hit(player->entityId())) {
            viewers.push_back(world::triggerFacts::ofPlayer(*player));
        }
    }
    ::basalt::server::world::loot::EntityFacts facts = world::triggerFacts::ofEntity(world, bolt);
    facts.blocksSetOnFire = bolt.boltFiresSet;
    for (const PlayerSession* player : watchers) {
        const double dx = player->x - bolt.x;
        const double dy = player->y - bolt.y;
        const double dz = player->z - bolt.z;
        if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) >= kAnnounceDistance) {
            continue;
        }
        // parity: `LightningStrikeTrigger` wants any one viewer to match, so each is offered in turn.
        if (viewers.empty()) {
            triggers_->fire("lightning_strike", player->name(), player->uuid().toString(),
                            world::TriggerEvent{.entities = {{"lightning", facts}}});
        }
        for (const ::basalt::server::world::loot::EntityFacts& viewer : viewers) {
            triggers_->fire("lightning_strike", player->name(), player->uuid().toString(),
                            world::TriggerEvent{.entities = {{"lightning", facts}, {"bystander", viewer}}});
        }
    }
}

} // namespace basalt::server::runtime
