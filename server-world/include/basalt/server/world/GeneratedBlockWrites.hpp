#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTypes.hpp>

#include <cstdint>
#include <unordered_set>

namespace basalt::server::world::generatedBlockWrites {

/// Cells a batch has already written. parity: in vanilla a cross-border write lands in a proto
using BatchWritten = std::unordered_set<std::int64_t>;

[[nodiscard]] std::int64_t cellKey(const ChunkGeneratedBlockWrite& write);

bool apply(Chunk& chunk, const ChunkGeneratedBlockWrite& write, bool sourceGoesOnTop,
           const BatchWritten* queued = nullptr);

} // namespace basalt::server::world::generatedBlockWrites
