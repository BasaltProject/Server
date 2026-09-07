#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkTypes.hpp>

#include <cstdint>
#include <unordered_set>

namespace basalt::server::world::generatedBlockWrites {

/// Cells a batch has already written. parity: in vanilla a cross-border write lands in a proto
/// chunk that is always there, so it sits under whatever that chunk's own decoration puts on top.
/// Only the batch's earlier writes may be overwritten, because among themselves they keep the order
/// they were issued in.
using BatchWritten = std::unordered_set<std::int64_t>;

[[nodiscard]] std::int64_t cellKey(const ChunkGeneratedBlockWrite& write);

/// `sourceGoesOnTop` is whether the writing chunk decorates after this one. When it does the write
/// lands over whatever is here, subject to the feature's own target test, which is what vanilla does
/// with a chunk already carved and decorated next to it. When it does not, the receiver's decoration
/// came later and would have covered the write, so only a cell nobody else touched takes it.
bool apply(Chunk& chunk, const ChunkGeneratedBlockWrite& write, bool sourceGoesOnTop,
           const BatchWritten* queued = nullptr);

} // namespace basalt::server::world::generatedBlockWrites
