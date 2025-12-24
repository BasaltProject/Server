#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/NeighbourUpdates.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;

/// parity: `Explosion.isSmall`, which is what picks the small flash over the big one.
constexpr float kLargeExplosion = 2.0F;

/// parity: `ItemEntity.merge(stack, input, 16)`, the cap on what one blast stacks together.
constexpr std::int32_t kExplosionMergeLimit = 16;

// parity: `Block.popResource` and `ItemEntity`'s own opening movement.
constexpr double kPopJitter = 0.25;
constexpr double kPopSpread = 0.2;
constexpr double kPopLift = 0.2;
constexpr double kItemHalfHeight = 0.125;

void sendBlock(const std::vector<player::PlayerSession*>& watchers, std::int32_t x, std::int32_t y, std::int32_t z,
               std::int32_t stateId) {
    const play::ClientboundBlockUpdatePacket packet{.pos = play::BlockPos{x, y, z}, .blockStateId = stateId};
    for (const player::PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::ClientboundBlockUpdatePacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
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
                             double y, double z, float power, std::string_view cause, std::int32_t sourceEntityId) {
    if (hooks_.allowed && !hooks_.allowed(world, x, y, z, power, cause, sourceEntityId)) {
        return;
    }
    const config::DimensionFacts& facts = config::dimensionFacts(world.declaration().dimension);
    const std::int64_t seed = options_.nextSeed ? options_.nextSeed() : 0;
    random::RandomSource random = random::RandomSource::legacy(seed);
    const std::vector<world::explosions::Broken> broken = world::explosions::explode(
        world.state(), facts.chunkHeight.minY(), facts.chunkHeight.maxY(), x, y, z, power, random);

    // parity: `ServerExplosion.createDrops`, which gathers everything the blast broke and only then
    // drops it: what an explosion leaves is a handful of merged stacks, not one item per block, and
    // that is what a real client shows.
    std::vector<Drop> drops;
    // parity: `ServerLevel.explode`'s own packet, which is the flash, the smoke thrown off the
    // blocks and the bang. The blocks themselves go out as ordinary updates below, so without this
    // an explosion was a wall of stone silently turning into air.
    const play::ExplodePacket explode{
        .x = x,
        .y = y,
        .z = z,
        .radius = power,
        .blockCount = static_cast<std::int32_t>(broken.size()),
        .particleId = particleId(power < kLargeExplosion ? "explosion" : "explosion_emitter"),
        .sound = "entity.generic.explode",
        .blockParticles = {
            play::ExplosionParticle{
                .particleId = particleId("poof"), .scaling = play::ExplodePacket::kPoofScaling, .speed = 1.0F},
            play::ExplosionParticle{
                .particleId = particleId("smoke"), .scaling = play::ExplodePacket::kSmokeScaling, .speed = 1.0F}}};
    for (const player::PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::ExplodePacket::kId,
                                                [&explode](wire::io::ByteBuffer& buffer) { explode.write(buffer); });
        }
    }

    for (const world::explosions::Broken& block : broken) {
        sendBlock(watchers, block.x, block.y, block.z, reg::blocks::air().stateId);
        if (const std::optional<reg::BlockState> state = reg::blocks::byStateId(block.stateId); state.has_value()) {
            collectExplodedBlock(state->block, block.x, block.y, block.z, power, drops);
        }
    }

    const world::neighbourUpdates::Context context{.world = world.state(),
                                                   .worldName = world.name(),
                                                   .minY = facts.chunkHeight.minY(),
                                                   .maxY = facts.chunkHeight.maxY(),
                                                   .random = random,
                                                   .hasSkylight = facts.hasSkylight};
    std::vector<world::BlockChange> effects;
    for (const world::explosions::Broken& block : broken) {
        world::neighbourUpdates::afterChange(
            context, block.x, block.y, block.z, effects,
            [this, &watchers, &drops](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
                sendBlock(watchers, bx, by, bz, reg::blocks::air().stateId);
                // What the blast knocked loose afterwards falls as an ordinary break, not as part of
                // the explosion: parity with `dropResources`, which carries no radius.
                if (const std::optional<reg::BlockState> state = reg::blocks::byStateId(stateId); state.has_value()) {
                    collectExplodedBlock(state->block, bx, by, bz, 0.0F, drops);
                }
            },
            [&watchers](std::int32_t stateId, std::int32_t bx, std::int32_t by, std::int32_t bz) {
                sendBlock(watchers, bx, by, bz, stateId);
            });
    }

    for (const Drop& drop : drops) {
        popResource(world, drop);
    }
    for (const player::PlayerSession* player : watchers) {
        const double distance = std::sqrt((player->x - x) * (player->x - x) + (player->y - y) * (player->y - y) +
                                          (player->z - z) * (player->z - z));
        const float damage = world::explosions::damageAt(distance, power, power * 2.0F);
        if (damage > 0.0F && player->connection().hurt) {
            player->connection().hurt(damage, "explosion", world::damageTypes::kExplosion, std::nullopt);
        }
    }
}

void ExplosionSystem::collectExplodedBlock(const Identifier& block, std::int32_t x, std::int32_t y, std::int32_t z,
                                           float power, std::vector<Drop>& drops) const {
    const server::world::loot::LootTable* table = lootTables_->forBlock(block);
    if (table == nullptr) {
        return;
    }
    // parity: `LootContextParams.EXPLOSION_RADIUS`, which `explosion_decay` turns into the one in
    // `power` chance each item has of surviving the blast.
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    for (const server::world::loot::ItemStack& stack : table->generate(random, 0.0F, power)) {
        if (reg::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        // parity: `ServerExplosion.addOrAppendStack`, which merges into the first stack that will
        // take it, up to sixteen, and only starts a new one when none will.
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
