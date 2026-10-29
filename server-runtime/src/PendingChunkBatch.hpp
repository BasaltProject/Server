#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTracker.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace basalt::server::runtime::pendingChunkBatch {

// A generation request is not a delivery. Keep every position until it is ready to send,
// including chunks still generating, evicted from the generation queue, or awaiting neighbours.
template <typename ReadyChunk>
[[nodiscard]] std::vector<server::world::Chunk*> takeReady(std::vector<server::world::ChunkPos>& pending,
                                                          std::int32_t limit, ReadyChunk&& readyChunk) {
    std::vector<server::world::Chunk*> ready;
    std::vector<server::world::ChunkPos> left;
    for (const server::world::ChunkPos& pos : pending) {
        if (static_cast<std::int32_t>(ready.size()) >= limit) {
            left.push_back(pos);
            continue;
        }
        if (server::world::Chunk* chunk = readyChunk(pos); chunk != nullptr) {
            ready.push_back(chunk);
        } else {
            left.push_back(pos);
        }
    }
    pending = std::move(left);
    return ready;
}

} // namespace basalt::server::runtime::pendingChunkBatch
