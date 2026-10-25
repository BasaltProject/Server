#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
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
    for (const player::PlayerSession* player : watchers) {
        if (player->connection().sendExplosion) {
            player->connection().sendExplosion(x, y, z);
        }
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::ExplodePacket::kId,
                                                [&explode](wire::io::ByteBuffer& buffer) { explode.write(buffer); });
        }
    }

    // parity: `ServerExplosion.explode`.
    for (const world::DroppedItem& item : world.state().items()) {
        const double distance =
            std::sqrt((item.x - x) * (item.x - x) + (item.y - y) * (item.y - y) + (item.z - z) * (item.z - z));
        const float damage = world::explosions::damageAt(distance, power, power * 2.0F);
        if (damage <= 0.0F) {
            continue;
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
                                              world.settings().rules.gameplay.fireTickDelay +
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
        const float damage = world::explosions::damageAt(distance, power, power * 2.0F);
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
