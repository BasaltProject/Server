#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/SculkCatalystSystem.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/BlockEntityDefaults.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/SculkSpreading.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>

namespace basalt::server::runtime {
namespace {

using player::PlayerSession;
using wire::common::Identifier;
namespace sculk = world::sculkSpreading;

const Identifier& catalystType() {
    static const Identifier type = Identifier::vanilla("sculk_catalyst");
    return type;
}

/// parity: the block entity a sensor or shrieker grown by the spreader comes with.
void ensureBlockEntity(world::LoadedWorld& world, const std::array<std::int32_t, 3>& pos, std::int32_t stateId) {
    const Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || (block->path() != "sculk_sensor" && block->path() != "sculk_shrieker")) {
        return;
    }
    server::world::Chunk* chunk = world.state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
    if (chunk == nullptr || chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15) != nullptr) {
        return;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{pos[0] & 15, pos[1], pos[2] & 15, *block,
                                                          world::blockEntityDefaults::of(*block, stateId)});
}

} // namespace

SculkCatalystSystem::SculkCatalystSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                                         Options options)
    : players_(&players), broadcaster_(&broadcaster), options_(std::move(options)) {}

std::vector<PlayerSession*> SculkCatalystSystem::watchersOf(const world::LoadedWorld& world) const {
    std::vector<PlayerSession*> watchers;
    for (PlayerSession* player : players_->snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    return watchers;
}

bool SculkCatalystSystem::tick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    server::world::Chunk* chunk = world.state().chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* entity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (entity == nullptr) {
        return false;
    }
    std::vector<sculk::Cursor> cursors = sculk::read(entity->data());
    if (cursors.empty()) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    const world::realBlockTags::TagMembers* tags = options_.blockTags;
    const sculk::BlockTagged tagged = [tags](std::string_view tag, std::int32_t stateId) {
        const Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
        return tags != nullptr && block != nullptr && world::realBlockTags::contains(*tags, tag, *block);
    };
    std::vector<sculk::Step> steps;
    sculk::update(world.state(), cursors, {x, y, z}, random, tagged, steps);
    // The spread may have rewritten the chunk's block entities, so the catalyst is looked up again.
    if (server::world::ChunkBlockEntity* again = chunk->blockEntityAt(x & 15, y, z & 15); again != nullptr) {
        sculk::write(cursors, again->data());
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    for (const sculk::Step& step : steps) {
        const auto& [sx, sy, sz] = step.pos;
        if (step.stateId.has_value()) {
            ensureBlockEntity(world, step.pos, *step.stateId);
            const world::BlockChange change{.x = sx, .y = sy, .z = sz, .stateId = *step.stateId};
            if (step.updatesNeighbours) {
                broadcaster_->announceRedstone(world, watchers, change);
            } else {
                // parity: flag two, the clients and the shape walk but no neighbour told.
                broadcaster_->announceBlock(watchers, change);
                broadcaster_->reshapeAround(world, watchers, sx, sy, sz);
            }
        } else if (step.effect.has_value()) {
            broadcaster_->sendEffects(watchers, sx, sy, sz, world.state().blockState(sx, sy, sz).value_or(0),
                                      {*step.effect});
        }
    }
    chunk->markDirty();
    return true;
}

std::optional<std::array<std::int32_t, 3>> SculkCatalystSystem::nearest(const world::LoadedWorld& world, double x,
                                                                        double y, double z) const {
    const auto reach = static_cast<std::int32_t>(sculk::kListenerRadius);
    const std::int32_t minChunkX = (wire::common::mth::floor(x) - reach) >> 4;
    const std::int32_t maxChunkX = (wire::common::mth::floor(x) + reach) >> 4;
    const std::int32_t minChunkZ = (wire::common::mth::floor(z) - reach) >> 4;
    const std::int32_t maxChunkZ = (wire::common::mth::floor(z) + reach) >> 4;
    std::optional<std::array<std::int32_t, 3>> best;
    double bestDistance = sculk::kListenerRadius * sculk::kListenerRadius;
    for (std::int32_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
        for (std::int32_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ) {
            const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                if (entity.type() != catalystType()) {
                    continue;
                }
                const std::int32_t bx = (chunkX << 4) + entity.localX();
                const std::int32_t bz = (chunkZ << 4) + entity.localZ();
                const double dx = bx + 0.5 - x;
                const double dy = entity.y() + 0.5 - y;
                const double dz = bz + 0.5 - z;
                const double distance = (dx * dx) + (dy * dy) + (dz * dz);
                // parity: the listener hears within its radius, and the nearest is told first.
                if (distance <= bestDistance && (!best.has_value() || distance < bestDistance)) {
                    bestDistance = distance;
                    best = std::array<std::int32_t, 3>{bx, entity.y(), bz};
                }
            }
        }
    }
    return best;
}

void SculkCatalystSystem::drink(world::LoadedWorld& world, const std::array<std::int32_t, 3>& catalyst, double x,
                                double y, double z, std::optional<std::int32_t> experience) {
    const auto& [cx, cy, cz] = catalyst;
    if (experience.has_value() && *experience > 0) {
        server::world::Chunk* chunk = world.state().chunkIfLoaded(cx >> 4, cz >> 4);
        if (server::world::ChunkBlockEntity* entity =
                chunk == nullptr ? nullptr : chunk->blockEntityAt(cx & 15, cy, cz & 15);
            entity != nullptr) {
            std::vector<sculk::Cursor> cursors = sculk::read(entity->data());
            // parity: `BlockPos.containing(sourcePosition.relative(UP, 0.5))`.
            sculk::addCursors(
                cursors, {wire::common::mth::floor(x), wire::common::mth::floor(y + 0.5), wire::common::mth::floor(z)},
                *experience);
            sculk::write(cursors, entity->data());
            chunk->markDirty();
        }
    }
    // parity: `CatalystListener.bloom`.
    const std::optional<std::int32_t> state = world.state().blockState(cx, cy, cz);
    const std::optional<std::int32_t> bloom = state.has_value() ? sculk::bloomed(*state) : std::nullopt;
    if (!bloom.has_value()) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    random::RandomSource random = random::RandomSource::legacy(options_.nextSeed ? options_.nextSeed() : 0);
    if (world.state().setBlockState(cx, cy, cz, *bloom)) {
        broadcaster_->announceRedstone(world, watchers,
                                       world::BlockChange{.x = cx, .y = cy, .z = cz, .stateId = *bloom});
    }
    world.state().scheduleBlockTick(cx, cy, cz, catalystType(), sculk::kBloomTicks);
    if (const server::registry::ParticleType* soul =
            server::registry::particleTypes::of(Identifier::vanilla("sculk_soul"));
        soul != nullptr) {
        const wire::protocol::play::LevelParticlesPacket packet{.particleTypeId = soul->protocolId,
                                                                .x = cx + 0.5,
                                                                .y = cy + 1.15,
                                                                .z = cz + 0.5,
                                                                .offsetX = 0.2F,
                                                                .offsetZ = 0.2F,
                                                                .count = 2};
        for (const PlayerSession* player : watchers) {
            player::showParticleFor(*player, soul->id.toString(), packet);
        }
    }
    broadcaster_->sendEffects(watchers, cx, cy, cz, *bloom,
                              {world::blockEffect::Sound{.name = "minecraft:block.sculk_catalyst.bloom",
                                                         .volume = 2.0F,
                                                         .pitch = 0.6F + (random.nextFloat() * 0.4F)}});
}

bool SculkCatalystSystem::scheduledTick(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
    const std::optional<std::int32_t> calm = state.has_value() ? sculk::unbloomed(*state) : std::nullopt;
    if (!calm.has_value() || !world.state().setBlockState(x, y, z, *calm)) {
        return false;
    }
    broadcaster_->announceRedstone(world, watchersOf(world),
                                   world::BlockChange{.x = x, .y = y, .z = z, .stateId = *calm});
    return true;
}

} // namespace basalt::server::runtime
