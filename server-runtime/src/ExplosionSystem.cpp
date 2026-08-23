#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;

/// parity: `Explosion.isSmall`, which is what picks the small flash over the big one.
constexpr float kLargeExplosion = 2.0F;

/// parity: `ItemEntity.merge(stack, input, 16)`.
constexpr std::int32_t kExplosionMergeLimit = 16;

// parity: `Block.popResource` and `ItemEntity`'s own opening movement.
constexpr double kPopJitter = 0.25;
constexpr double kPopSpread = 0.2;
constexpr double kPopLift = 0.2;
constexpr double kItemHalfHeight = 0.125;

[[nodiscard]] double playerHeightOf(const play::Pose pose) {
    switch (pose) {
    case play::Pose::Sleeping:
        return 0.2;
    case play::Pose::FallFlying:
    case play::Pose::Swimming:
    case play::Pose::SpinAttack:
        return 0.6;
    case play::Pose::Crouching:
        return 1.5;
    default:
        return 1.8;
    }
}

constexpr double kPlayerWidth = 0.6;
/// parity: `Enchantments.BLAST_PROTECTION`'s `EXPLOSION_KNOCKBACK_RESISTANCE` per level.
constexpr double kBlastResistancePerLevel = 0.15;

[[nodiscard]] double eyeHeightOf(const player::PlayerSession& player) {
    switch (player.pose) {
    case play::Pose::Sleeping:
        return 0.2;
    case play::Pose::FallFlying:
    case play::Pose::Swimming:
    case play::Pose::SpinAttack:
        return 0.4;
    case play::Pose::Crouching:
        return 1.27;
    default:
        return 1.62;
    }
}

[[nodiscard]] double blastResistance(const player::PlayerSession& player) {
    const Identifier blast = Identifier::vanilla("blast_protection");
    std::int32_t levels = 0;
    for (std::int32_t slot = player::PlayerInventory::kArmourStart;
         slot < player::PlayerInventory::kArmourStart + player::PlayerInventory::kArmourSlots; slot++) {
        const std::optional<storage::StoredItemStack> worn = player.inventory.at(slot);
        if (!worn.has_value()) {
            continue;
        }
        for (const auto& [id, level] : player::enchantmentLevels(*worn)) {
            levels += id == blast ? level : 0;
        }
    }
    return std::clamp(static_cast<double>(levels) * kBlastResistancePerLevel, 0.0, 1.0);
}

constexpr double kItemSize = 0.25;

[[nodiscard]] world::explosions::Box boxAround(double x, double y, double z, double width, double height) {
    return {x - (width / 2.0), y, z - (width / 2.0), x + (width / 2.0), y + height, z + (width / 2.0)};
}

/// parity: `ServerExplosion.hurtEntities` weighs every entity before `interactWithBlocks` breaks anything.
void measureExposures(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                      double y, double z, float power) {
    world::explosions::clearExposures();
    const double doubleRadius = static_cast<double>(power) * 2.0;
    const auto within = [&](double ex, double ey, double ez) {
        return std::sqrt((ex - x) * (ex - x) + (ey - y) * (ey - y) + (ez - z) * (ez - z)) / doubleRadius <= 1.0;
    };
    for (const player::PlayerSession* player : watchers) {
        if (within(player->x, player->y, player->z)) {
            world::explosions::recordExposure(
                player->entityId(),
                world::explosions::seenPercent(
                    world.state(), x, y, z,
                    boxAround(player->x, player->y, player->z, kPlayerWidth, playerHeightOf(player->pose))));
        }
    }
    for (const entity::ServerEntity* entity : world.state().entities()) {
        if (entity->removed || !within(entity->x, entity->y, entity->z)) {
            continue;
        }
        world::explosions::recordExposure(
            entity->entityId(),
            world::explosions::seenPercent(
                world.state(), x, y, z, boxAround(entity->x, entity->y, entity->z, entity->width(), entity->height())));
    }
    for (const world::DroppedItem& item : world.state().items()) {
        if (within(item.x, item.y, item.z)) {
            world::explosions::recordExposure(
                item.entityId, world::explosions::seenPercent(world.state(), x, y, z,
                                                              boxAround(item.x, item.y, item.z, kItemSize, kItemSize)));
        }
    }
}

void sendBlock(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId) {
    for (const player::PlayerSession* player : watchers) {
        player::sendBlockChangeTo(*player, x, y, z, stateId);
    }
}

} // namespace

ExplosionSystem::ExplosionSystem(player::PlayerRegistry& players, world::RealLootTables& lootTables, Hooks hooks,
                                 Options options)
    : players_(&players), lootTables_(&lootTables), hooks_(std::move(hooks)), options_(std::move(options)) {}

std::int32_t ExplosionSystem::particleId(std::string_view path) {
    const reg::ParticleType* type = reg::particleTypes::of(Identifier::vanilla(std::string{path}));
    return type == nullptr ? 0 : type->protocolId;
}

double ExplosionSystem::jitter() const {
    const double roll = options_.nextDouble ? options_.nextDouble() : 0.5;
    return roll * 2 * kPopJitter - kPopJitter;
}

void ExplosionSystem::blowUp(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, double x,
                             double y, double z, float power, std::string_view cause, std::int32_t sourceEntityId,
                             const world::explosions::Spares& spares, const world::explosions::Resistance& resistance,
                             bool fire) {
    if (hooks_.allowed && !hooks_.allowed(world, x, y, z, power, cause, sourceEntityId)) {
        return;
    }
    measureExposures(world, watchers, x, y, z, power);
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    random::RandomSource random = random::RandomSource::legacy(seed);
    const std::vector<world::explosions::Broken> broken = world::explosions::explode(
        world.state(), facts.chunkHeight.minY(), facts.chunkHeight.maxY(), x, y, z, power, random, spares, resistance);

    // parity: `ServerExplosion.createDrops`.
    std::vector<Drop> drops;
    // parity: `ServerLevel.explode`.
    // parity: `ServerLevel.explode`.
    const entity::ServerEntity* source = sourceEntityId == 0 ? nullptr : world.state().entity(sourceEntityId);
    const bool silent = source != nullptr && source->sourceNbt.has_value() && source->sourceNbt->getBoolean("Silent");
    const play::ExplodePacket explode{
        .x = x,
        .y = y,
        .z = z,
        .radius = power,
        .blockCount = static_cast<std::int32_t>(broken.size()),
        .particleId = particleId(power < kLargeExplosion ? "explosion" : "explosion_emitter"),
        .sound = "entity.generic.explode",
        .blockParticles = {play::ExplosionParticle{.particleId = particleId("poof"),
                                                   .scaling = play::ExplodePacket::kPoofScaling,
                                                   .speed = 1.0F},
                           play::ExplosionParticle{.particleId = particleId("smoke"),
                                                   .scaling = play::ExplodePacket::kSmokeScaling,
                                                   .speed = 1.0F}},
        .playSound = !silent};
    const double doubleRadius = static_cast<double>(power) * 2.0;
    for (player::PlayerSession* player : watchers) {
        play::ExplodePacket own = explode;
        const double distance = std::sqrt((player->x - x) * (player->x - x) + (player->y - y) * (player->y - y) +
                                          (player->z - z) * (player->z - z)) /
                                doubleRadius;
        if (distance <= 1.0 && !world.settings().rules.environment.disableExplosionKnockback) {
            const std::array<double, 3> push = world::explosions::knockbackOf(
                x, y, z, player->x, player->y + eyeHeightOf(*player), player->z, distance,
                world::explosions::exposureOf(player->entityId()), blastResistance(*player));
            player->deltaMovement[0] += push[0];
            player->deltaMovement[1] += push[1];
            player->deltaMovement[2] += push[2];
            if (player->gamemode != play::Gamemode::Spectator &&
                (player->gamemode != play::Gamemode::Creative || !player->flying)) {
                own.knockbackX = push[0];
                own.knockbackY = push[1];
                own.knockbackZ = push[2];
            }
        }
        if (player->connection().sendExplosion) {
            player->connection().sendExplosion(x, y, z);
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::ExplodePacket::kId,
                                                [&own](wire::io::ByteBuffer& buffer) { own.write(buffer); });
        }
    }

    // parity: `ServerExplosion.explode`.
    for (const world::DroppedItem& item : world.state().items()) {
        const double distance =
            std::sqrt((item.x - x) * (item.x - x) + (item.y - y) * (item.y - y) + (item.z - z) * (item.z - z));
        if (distance / doubleRadius > 1.0) {
            continue;
        }
        const float exposure = world::explosions::exposureOf(item.entityId);
        const float damage = world::explosions::damageAt(distance, static_cast<float>(doubleRadius), exposure);
        if (!world.settings().rules.environment.disableExplosionKnockback) {
            const std::array<double, 3> push = world::explosions::knockbackOf(
                x, y, z, item.x, item.y + (kItemSize * 0.85), item.z, distance / doubleRadius, exposure, 0.0);
            static_cast<void>(world.state().pushItem(item.entityId, push[0], push[1], push[2]));
        }
        static_cast<void>(world.state().hurtItem(item.entityId, static_cast<std::int32_t>(damage)));
    }

    if (hooks_.struck) {
        hooks_.struck(world, watchers, x, y, z, power, sourceEntityId);
    }

    for (const world::explosions::Broken& block : broken) {
        sendBlock(watchers, block.x, block.y, block.z, reg::blocks::air().stateId);
        if (const std::optional<reg::BlockState> state = reg::blocks::byStateId(block.stateId); state.has_value()) {
            collectExplodedBlock(world, state->block, block.stateId, block.x, block.y, block.z, power, drops);
        }
        if (block.hive.has_value() && hooks_.hiveBlown) {
            hooks_.hiveBlown(world, block.x, block.y, block.z, block.stateId, *block.hive, sourceEntityId);
        }
        // parity: `JukeboxBlockEntity.setRemoved`.
        if (block.songWasPlaying) {
            for (const player::PlayerSession* watcher : watchers) {
                if (watcher->connection().sendLevelEvent) {
                    watcher->connection().sendLevelEvent(world::blockEffect::Effect::kStopJukeboxSong, block.x, block.y,
                                                         block.z, 0);
                }
            }
            world::vibrations::emit(world.state(), "jukebox_stop_play", block.x + 0.5, block.y + 0.5, block.z + 0.5,
                                    world.gameTime());
        }
    }

    const world::neighbourUpdates::Context context{.world = world.state(),
                                                   .worldName = world.name(),
                                                   .minY = facts.chunkHeight.minY(),
                                                   .maxY = facts.chunkHeight.maxY(),
                                                   .random = random,
                                                   .hasSkylight = facts.hasSkylight,
                                                   .gameTime = world.gameTime(),
                                                   .tntExplodes =
                                                       world.gameRules().get(world::gameRules::kTntExplodes)};
    std::vector<world::BlockChange> effects;
    for (const world::explosions::Broken& block : broken) {
        world::neighbourUpdates::afterChange(
            context, block.x, block.y, block.z, block.stateId, effects,
            [this, &world, &watchers, &drops](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
                sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
                // parity: `dropResources` has no radius.
                if (const std::optional<reg::BlockState> state = reg::blocks::byStateId(stateId); state.has_value()) {
                    collectExplodedBlock(world, state->block, stateId, bx, by, bz, 0.0F, drops);
                }
            },
            [&watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
                sendBlock(watchers, bx, by, bz, stateId);
            });
    }

    for (const Drop& drop : drops) {
        popResource(world, drop);
    }
    // parity: `ServerExplosion.createFire`.
    if (fire) {
        for (const world::explosions::Broken& block : broken) {
            if (random.nextInt(3) != 0 ||
                !world::fires::igniteOnImpact(world.state(), block.x, block.y, block.z,
                                              world.settings().rules.tuning.fireTickDelay +
                                                  random.nextInt(world::fires::kDelaySpread))) {
                continue;
            }
            if (const std::optional<std::int32_t> lit = world.state().blockState(block.x, block.y, block.z);
                lit.has_value()) {
                sendBlock(watchers, block.x, block.y, block.z, *lit);
            }
        }
    }
    for (player::PlayerSession* player : watchers) {
        const double distance = std::sqrt((player->x - x) * (player->x - x) + (player->y - y) * (player->y - y) +
                                          (player->z - z) * (player->z - z));
        // parity: `ServerExplosion.hurtEntities` calls `onExplosionHit` on whatever is inside the blast.
        if (distance / (static_cast<double>(power) * 2.0) <= 1.0 && player->gamemode != play::Gamemode::Spectator) {
            player->explosionImpact = std::array<double, 3>{player->x, player->y, player->z};
            player->explosionCause = sourceEntityId == 0 ? std::nullopt : std::optional{sourceEntityId};
        }
        const float damage = distance / doubleRadius > 1.0
                                 ? 0.0F
                                 : world::explosions::damageAt(distance, static_cast<float>(doubleRadius),
                                                               world::explosions::exposureOf(player->entityId()));
        if (damage > 0.0F && player->connection().hurt) {
            player->connection().hurt(damage, "explosion", world::damageTypes::kExplosion, std::nullopt);
        }
    }
}

void ExplosionSystem::collectExplodedBlock(world::LoadedWorld& world, const Identifier& block, std::int32_t stateId,
                                           std::int32_t x, std::int32_t y, std::int32_t z, float power,
                                           std::vector<Drop>& drops) const {
    const server::world::loot::LootTable* table = lootTables_->forBlock(block);
    if (table == nullptr) {
        return;
    }
    // parity: `LootContextParams.EXPLOSION_RADIUS`.
    random::RandomSource scratch = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    random::RandomSource& random =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
    server::world::loot::LootContext context{.blockState = reg::blockStateDefinitions::propertiesOf(stateId)};
    if (const Identifier* broken = reg::blockStateDefinitions::blockOf(stateId); broken != nullptr) {
        context.block = *broken;
    }
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, power, context)) {
        if (reg::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        // parity: `ServerExplosion.addOrAppendStack`.
        std::int32_t remaining = stack.count;
        for (Drop& drop : drops) {
            if (drop.item != stack.item || drop.count >= kExplosionMergeLimit) {
                continue;
            }
            const std::int32_t moved = std::min(remaining, kExplosionMergeLimit - drop.count);
            drop.count += moved;
            remaining -= moved;
            if (remaining <= 0) {
                break;
            }
        }
        if (remaining > 0) {
            drops.push_back(Drop{.item = stack.item, .count = remaining, .x = x, .y = y, .z = z});
        }
    }
}

void ExplosionSystem::popResource(world::LoadedWorld& world, const Drop& drop) const {
    // parity: `Block.popResource`.
    if (!world.gameRules().get(world::gameRules::kBlockDrops)) {
        return;
    }
    const double spreadX = (options_.nextDouble ? options_.nextDouble() : 0.5) * kPopSpread - kPopSpread / 2.0;
    const double spreadZ = (options_.nextDouble ? options_.nextDouble() : 0.5) * kPopSpread - kPopSpread / 2.0;
    static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                                 .uuid = wire::common::Uuid::random(),
                                                                 .item = drop.item,
                                                                 .count = drop.count,
                                                                 .x = drop.x + 0.5 + jitter(),
                                                                 .y = drop.y + 0.5 + jitter() - kItemHalfHeight,
                                                                 .z = drop.z + 0.5 + jitter(),
                                                                 .velocityX = spreadX,
                                                                 .velocityY = kPopLift,
                                                                 .velocityZ = spreadZ}));
}

} // namespace basalt::server::runtime
