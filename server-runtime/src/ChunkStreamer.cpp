#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/ChunkStreamer.hpp>
#include <basalt/server/runtime/ClientSession.hpp>
#include <basalt/server/runtime/world/ChunkGenerationQueue.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/ChunkDataCodec.hpp>
#include <basalt/server/world/light/LightWindow.hpp>
#include <basalt/wire/protocol/play/SimplePackets.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using server::world::Chunk;
using server::world::ChunkPos;
using wire::common::Identifier;
using wire::protocol::play::ChunkBatchFinishedPacket;
using wire::protocol::play::ForgetLevelChunkPacket;
using wire::protocol::play::GameEventPacket;
using wire::protocol::play::MapChunkPacket;
using wire::protocol::play::SetChunkCacheCenterPacket;
using wire::protocol::play::SetChunkCacheRadiusPacket;
using wire::protocol::play::SetSimulationDistancePacket;
using wire::protocol::play::UpdateLightPacket;

/// parity: the square vanilla sends before it lets a player in, and how far ahead this one guesses.
constexpr std::int32_t kLoginChunkRadius = 2;
constexpr std::int32_t kPrefetchMarginChunks = 2;
constexpr std::int32_t kPrefetchPriorityPenalty = 1000;

} // namespace

ChunkStreamer::ChunkStreamer(ClientSession& owner)
    : desiredChunksPerTick(ClientSession::kInitialChunksPerTick), owner_(&owner) {}

std::int64_t ChunkStreamer::squaredDistanceToCenter(std::int32_t chunkX, std::int32_t chunkZ) const {
    const std::int64_t dx = chunkX - centerChunkX_;
    const std::int64_t dz = chunkZ - centerChunkZ_;
    return (dx * dx) + (dz * dz);
}

server::world::ChunkTracker::Delta ChunkStreamer::initialJoin(double x, double z, std::int32_t radius) {
    centerChunkX_ = static_cast<std::int32_t>(x) >> 4;
    centerChunkZ_ = static_cast<std::int32_t>(z) >> 4;
    viewRadius_ = radius;
    server::world::ChunkTracker::Delta initialChunks =
        chunkTracker_.update(centerChunkX_, centerChunkZ_, trackedRadius());
    owner_->sendCompressed(SetChunkCacheCenterPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetChunkCacheCenterPacket{.chunkX = centerChunkX_, .chunkZ = centerChunkZ_}.write(buffer);
    });
    owner_->sendCompressed(SetChunkCacheRadiusPacket::kId, [radius](wire::io::ByteBuffer& buffer) {
        SetChunkCacheRadiusPacket{.radius = radius}.write(buffer);
    });
    owner_->sendCompressed(SetSimulationDistancePacket::kId, [radius](wire::io::ByteBuffer& buffer) {
        SetSimulationDistancePacket{.simulationDistance = radius}.write(buffer);
    });
    return initialChunks;
}

void ChunkStreamer::sendInitialChunks(const server::world::ChunkTracker::Delta& initialChunks) {
    owner_->sendCompressed(GameEventPacket::kId, [](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = GameEventPacket::kLevelChunksLoadStart, .param = 0.0F}.write(buffer);
    });
    std::vector<std::pair<std::int32_t, std::int32_t>> awaited;
    for (std::int32_t dx = -(kLoginChunkRadius + 1); dx <= kLoginChunkRadius + 1; dx++) {
        for (std::int32_t dz = -(kLoginChunkRadius + 1); dz <= kLoginChunkRadius + 1; dz++) {
            awaited.emplace_back(centerChunkX_ + dx, centerChunkZ_ + dz);
        }
    }
    std::ranges::sort(awaited, [this](const auto& left, const auto& right) {
        return squaredDistanceToCenter(left.first, left.second) < squaredDistanceToCenter(right.first, right.second);
    });
    static_cast<void>(owner_->generationQueue().awaitAll(awaited));
    std::vector<Chunk*> loginChunks;
    for (const auto& [chunkX, chunkZ] : awaited) {
        if (std::abs(chunkX - centerChunkX_) > kLoginChunkRadius ||
            std::abs(chunkZ - centerChunkZ_) > kLoginChunkRadius) {
            continue;
        }
        Chunk* chunk = owner_->world().chunkIfLoaded(chunkX, chunkZ);
        if (chunk != nullptr) {
            loginChunks.push_back(chunk);
        }
    }
    owner_->sendCompressed(wire::protocol::play::kChunkBatchStartId, [](wire::io::ByteBuffer&) {});
    for (Chunk* chunk : loginChunks) {
        sendChunk(*chunk);
    }
    owner_->sendCompressed(ChunkBatchFinishedPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        ChunkBatchFinishedPacket{.batchSize = static_cast<std::int32_t>(loginChunks.size())}.write(buffer);
    });

    std::set<std::pair<std::int32_t, std::int32_t>> sent;
    for (const Chunk* chunk : loginChunks) {
        sent.emplace(chunk->chunkX(), chunk->chunkZ());
    }
    std::vector<ChunkPos> streaming;
    for (const ChunkPos& pos : initialChunks.toLoad) {
        if (!sent.contains({pos.x, pos.z})) {
            streaming.push_back(pos);
        }
    }
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        pendingChunks_.insert(pendingChunks_.end(), streaming.begin(), streaming.end());
    }
    for (const ChunkPos& pos : streaming) {
        owner_->generationQueue().request(pos.x, pos.z,
                                          static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
    }
}

void ChunkStreamer::forgetAll() {
    chunkTracker_.reset();
    const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
    pendingChunks_.clear();
}

void ChunkStreamer::resetForRespawn(double spawnX, double spawnZ) {
    centerChunkX_ = static_cast<std::int32_t>(spawnX) >> 4;
    centerChunkZ_ = static_cast<std::int32_t>(spawnZ) >> 4;
    chunkTracker_.reset();
    const server::world::ChunkTracker::Delta delta =
        chunkTracker_.update(centerChunkX_, centerChunkZ_, trackedRadius());
    owner_->sendCompressed(SetChunkCacheCenterPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetChunkCacheCenterPacket{.chunkX = centerChunkX_, .chunkZ = centerChunkZ_}.write(buffer);
    });
    owner_->sendCompressed(GameEventPacket::kId, [](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = GameEventPacket::kLevelChunksLoadStart, .param = 0.0F}.write(buffer);
    });
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        pendingChunks_.insert(pendingChunks_.end(), delta.toLoad.begin(), delta.toLoad.end());
    }
    for (const ChunkPos& pos : delta.toLoad) {
        owner_->generationQueue().request(pos.x, pos.z,
                                          static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
    }
}

void ChunkStreamer::resetForWorldSwitch(double x, double z, std::int32_t radius) {
    centerChunkX_ = static_cast<std::int32_t>(x) >> 4;
    centerChunkZ_ = static_cast<std::int32_t>(z) >> 4;
    viewRadius_ = radius;
    chunkTracker_.reset();
    const server::world::ChunkTracker::Delta delta =
        chunkTracker_.update(centerChunkX_, centerChunkZ_, trackedRadius());
    owner_->sendCompressed(SetChunkCacheCenterPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetChunkCacheCenterPacket{.chunkX = centerChunkX_, .chunkZ = centerChunkZ_}.write(buffer);
    });
    owner_->sendCompressed(SetChunkCacheRadiusPacket::kId, [this](wire::io::ByteBuffer& buffer) {
        SetChunkCacheRadiusPacket{.radius = viewRadius_}.write(buffer);
    });
    owner_->sendCompressed(GameEventPacket::kId, [](wire::io::ByteBuffer& buffer) {
        GameEventPacket{.event = GameEventPacket::kLevelChunksLoadStart, .param = 0.0F}.write(buffer);
    });
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        pendingChunks_.clear();
        pendingChunks_.insert(pendingChunks_.end(), delta.toLoad.begin(), delta.toLoad.end());
    }
    for (const ChunkPos& pos : delta.toLoad) {
        owner_->generationQueue().request(pos.x, pos.z,
                                          static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
    }
}

void ChunkStreamer::resendChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    world::LoadedWorld* world = owner_->currentWorld();
    Chunk* chunk = world == nullptr ? nullptr : world->state().chunkIfLoaded(chunkX, chunkZ);
    if (chunk == nullptr) {
        return;
    }
    owner_->sendPlayPacket(wire::protocol::play::kChunkBatchStartId, [](wire::io::ByteBuffer&) {});
    sendChunk(*chunk);
    owner_->sendPlayPacket(ChunkBatchFinishedPacket::kId, [](wire::io::ByteBuffer& buffer) {
        ChunkBatchFinishedPacket{.batchSize = 1}.write(buffer);
    });
}

void ChunkStreamer::sendBiomes(const std::vector<std::pair<std::int32_t, std::int32_t>>& chunks) {
    world::LoadedWorld* world = owner_->currentWorld();
    if (world == nullptr) {
        return;
    }
    std::vector<wire::protocol::play::ChunkBiomeData> columns;
    for (const auto& [chunkX, chunkZ] : chunks) {
        Chunk* chunk = world->state().chunkIfLoaded(chunkX, chunkZ);
        if (chunk == nullptr) {
            continue;
        }
        columns.push_back(wire::protocol::play::ChunkBiomeData{
            .chunkX = chunkX, .chunkZ = chunkZ, .biomes = server::world::chunkDataCodec::encodeBiomes(*chunk)});
    }
    if (columns.empty()) {
        return;
    }
    owner_->sendCompressed(wire::protocol::play::ChunksBiomesPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        wire::protocol::play::ChunksBiomesPacket{.chunks = columns}.write(buffer);
    });
}

void ChunkStreamer::sendChunk(Chunk& chunk) {
    world::LoadedWorld* world = owner_->currentWorld();
    const bool hasSkylight = world == nullptr || config::dimensionFacts(world->declaration().dimension).hasSkylight;
    const server::world::chunkDataCodec::LightData light = server::world::chunkDataCodec::encodeLight(
        chunk, hasSkylight,
        server::world::light::LightWindow{chunk, [world](std::int32_t chunkX, std::int32_t chunkZ) -> const Chunk* {
                                              return world == nullptr ? nullptr
                                                                      : world->state().chunkIfLoaded(chunkX, chunkZ);
                                          }});
    const wire::protocol::play::LightData wireLight{.skyLightMask = light.skyLightMask,
                                                    .blockLightMask = light.blockLightMask,
                                                    .emptySkyLightMask = light.emptySkyLightMask,
                                                    .emptyBlockLightMask = light.emptyBlockLightMask,
                                                    .skyLight = light.skyLight,
                                                    .blockLight = light.blockLight};
    std::vector<wire::protocol::play::ChunkBlockEntityInfo> blockEntities;
    for (const auto& blockEntity : chunk.blockEntities()) {
        const std::optional<std::int32_t> typeId = server::registry::blockEntityTypes::idOf(blockEntity.type());
        if (!typeId.has_value()) {
            if (loggedUnknownBlockEntities_.insert(blockEntity.type()).second) {
                owner_->logger().warn([&] {
                    return "No protocol ID for block entity type " + blockEntity.type().toString() +
                           "; leaving it out of the chunk.";
                });
            }
            continue;
        }
        blockEntities.push_back(wire::protocol::play::ChunkBlockEntityInfo{
            .packedXZ = static_cast<std::uint8_t>((blockEntity.localX() << 4) | blockEntity.localZ()),
            .y = static_cast<std::int16_t>(blockEntity.y()),
            .typeId = *typeId,
            .tag = blockEntity.data()});
    }
    const std::vector<std::uint8_t> chunkData = server::world::chunkDataCodec::encodeSections(chunk);
    const std::vector<std::int64_t> heightmap = server::world::chunkDataCodec::encodeHeightmap(chunk);
    owner_->sendCompressed(MapChunkPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        MapChunkPacket{.x = chunk.chunkX(),
                       .z = chunk.chunkZ(),
                       .heightmaps = {wire::protocol::play::Heightmap{
                           .type = wire::protocol::play::HeightmapType::MotionBlocking, .data = heightmap}},
                       .chunkData = chunkData,
                       .blockEntities = blockEntities,
                       .light = wireLight}
            .write(buffer);
    });
    owner_->sendCompressed(UpdateLightPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        UpdateLightPacket{.chunkX = chunk.chunkX(), .chunkZ = chunk.chunkZ(), .light = wireLight}.write(buffer);
    });
}

std::int32_t ChunkStreamer::trackedRadius() const {
    const std::int32_t cap = viewOverride_.value_or(viewRadius_);
    return std::clamp(requestedViewDistance_, kMinViewDistance, std::max(cap, kMinViewDistance));
}

std::vector<std::pair<std::int32_t, std::int32_t>> ChunkStreamer::sentChunks() const {
    const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
    std::vector<std::pair<std::int32_t, std::int32_t>> sent;
    for (const ChunkPos& pos : chunkTracker_.loaded()) {
        if (std::ranges::find(pendingChunks_, pos) == pendingChunks_.end()) {
            sent.emplace_back(pos.x, pos.z);
        }
    }
    return sent;
}

void ChunkStreamer::overrideViewDistance(const std::optional<std::int32_t> distance) {
    viewOverride_ = distance;
    const std::int32_t radius = distance.value_or(viewRadius_);
    owner_->sendCompressed(wire::protocol::play::SetChunkCacheRadiusPacket::kId,
                           [radius](wire::io::ByteBuffer& buffer) {
                               wire::protocol::play::SetChunkCacheRadiusPacket{.radius = radius}.write(buffer);
                           });
    if (viewRadius_ > 0) {
        applyDelta(chunkTracker_.update(centerChunkX_, centerChunkZ_, trackedRadius()));
    }
}

void ChunkStreamer::setRequestedViewDistance(std::int32_t distance) {
    if (distance == requestedViewDistance_) {
        return;
    }
    requestedViewDistance_ = distance;
    if (viewRadius_ > 0) {
        applyDelta(chunkTracker_.update(centerChunkX_, centerChunkZ_, trackedRadius()));
    }
}

void ChunkStreamer::applyDelta(const server::world::ChunkTracker::Delta& delta) {
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        pendingChunks_.insert(pendingChunks_.end(), delta.toLoad.begin(), delta.toLoad.end());
        for (const ChunkPos& pos : delta.toUnload) {
            std::erase(pendingChunks_, pos);
        }
    }
    for (const ChunkPos& pos : delta.toLoad) {
        owner_->generationQueue().request(pos.x, pos.z,
                                          static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
    }
    for (const ChunkPos& pos : delta.toUnload) {
        owner_->sendCompressed(ForgetLevelChunkPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            ForgetLevelChunkPacket{.chunkX = pos.x, .chunkZ = pos.z}.write(buffer);
        });
    }
}

void ChunkStreamer::streamChunksAround(double x, double z) {
    const std::int32_t chunkX = static_cast<std::int32_t>(x) >> 4;
    const std::int32_t chunkZ = static_cast<std::int32_t>(z) >> 4;
    if (chunkX == centerChunkX_ && chunkZ == centerChunkZ_) {
        return;
    }
    const std::int32_t previousCenterX = centerChunkX_;
    const std::int32_t previousCenterZ = centerChunkZ_;
    centerChunkX_ = chunkX;
    centerChunkZ_ = chunkZ;

    const server::world::ChunkTracker::Delta delta = chunkTracker_.update(chunkX, chunkZ, trackedRadius());
    if (delta.toLoad.empty() && delta.toUnload.empty()) {
        return;
    }
    owner_->sendCompressed(SetChunkCacheCenterPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        SetChunkCacheCenterPacket{.chunkX = chunkX, .chunkZ = chunkZ}.write(buffer);
    });
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        pendingChunks_.insert(pendingChunks_.end(), delta.toLoad.begin(), delta.toLoad.end());
        for (const ChunkPos& pos : delta.toUnload) {
            std::erase(pendingChunks_, pos);
        }
    }
    for (const ChunkPos& pos : delta.toLoad) {
        owner_->generationQueue().request(pos.x, pos.z,
                                          static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
    }
    prefetchAhead(chunkX, chunkZ, chunkX - previousCenterX, chunkZ - previousCenterZ);
    // Unloads go out at once: the client can drop what it no longer needs before replacements are built.
    for (const ChunkPos& pos : delta.toUnload) {
        owner_->sendCompressed(ForgetLevelChunkPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            ForgetLevelChunkPacket{.chunkX = pos.x, .chunkZ = pos.z}.write(buffer);
        });
    }
    // No flush: the outbound loop is the only drainer, so the requested rate holds and reads never stall on encoding.
}

void ChunkStreamer::prefetchAhead(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t movedX, std::int32_t movedZ) {
    if (movedX == 0 && movedZ == 0) {
        return;
    }
    const std::int32_t stepX = std::clamp(movedX, -1, 1);
    const std::int32_t stepZ = std::clamp(movedZ, -1, 1);
    for (std::int32_t margin = 1; margin <= kPrefetchMarginChunks; margin++) {
        const std::int32_t aheadX = chunkX + (stepX * (viewRadius_ + margin));
        const std::int32_t aheadZ = chunkZ + (stepZ * (viewRadius_ + margin));
        for (std::int32_t offset = -viewRadius_; offset <= viewRadius_; offset++) {
            const std::int32_t x = stepX != 0 ? aheadX : chunkX + offset;
            const std::int32_t z = stepZ != 0 ? aheadZ : chunkZ + offset;
            owner_->generationQueue().request(
                x, z, static_cast<std::int32_t>(squaredDistanceToCenter(x, z)) + kPrefetchPriorityPenalty);
        }
    }
}

bool ChunkStreamer::neighboursLoaded(std::int32_t chunkX, std::int32_t chunkZ) {
    bool complete = true;
    for (std::int32_t offsetZ = -1; offsetZ <= 1; offsetZ++) {
        for (std::int32_t offsetX = -1; offsetX <= 1; offsetX++) {
            if (offsetX == 0 && offsetZ == 0) {
                continue;
            }
            const std::int32_t x = chunkX + offsetX;
            const std::int32_t z = chunkZ + offsetZ;
            if (owner_->world().isLoaded(x, z)) {
                continue;
            }
            complete = false;
            owner_->generationQueue().request(x, z, static_cast<std::int32_t>(squaredDistanceToCenter(x, z)));
        }
    }
    return complete;
}

std::int32_t ChunkStreamer::flushReadyChunks(std::int32_t limit) {
    if (owner_->state() != wire::protocol::ProtocolState::Play) {
        return 0;
    }
    std::vector<Chunk*> ready;
    {
        const std::lock_guard<std::mutex> guard{chunkStreamMutex_};
        if (pendingChunks_.empty()) {
            return 0;
        }
        std::ranges::sort(pendingChunks_, [this](const ChunkPos& left, const ChunkPos& right) {
            return squaredDistanceToCenter(left.x, left.z) < squaredDistanceToCenter(right.x, right.z);
        });
        std::vector<ChunkPos> left;
        for (const ChunkPos& pos : pendingChunks_) {
            if (static_cast<std::int32_t>(ready.size()) >= limit) {
                left.push_back(pos);
                continue;
            }
            Chunk* chunk = owner_->world().chunkIfLoaded(pos.x, pos.z);
            if (chunk == nullptr) {
                // Unloaded before it was sent: ask again, the tracker thinks the client has it and nothing else would.
                owner_->generationQueue().request(pos.x, pos.z,
                                                  static_cast<std::int32_t>(squaredDistanceToCenter(pos.x, pos.z)));
                continue;
            }
            if (!neighboursLoaded(pos.x, pos.z)) {
                left.push_back(pos);
                continue;
            }
            ready.push_back(chunk);
        }
        pendingChunks_ = std::move(left);
    }
    if (ready.empty()) {
        return 0;
    }
    owner_->sendCompressed(wire::protocol::play::kChunkBatchStartId, [](wire::io::ByteBuffer&) {});
    for (Chunk* chunk : ready) {
        sendChunk(*chunk);
    }
    owner_->sendCompressed(ChunkBatchFinishedPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        ChunkBatchFinishedPacket{.batchSize = static_cast<std::int32_t>(ready.size())}.write(buffer);
    });
    return static_cast<std::int32_t>(ready.size());
}

} // namespace basalt::server::runtime
