#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// Observes each accepted patch write, for callers that need to mirror them elsewhere.
using BlockWriteObserver = std::function<void(std::int32_t, std::int32_t, std::int32_t, std::int32_t)>;

/// The read and write view a step-9 patch feature works through.
///
/// The Kotlin repeated this class in thirteen decoration files; it is one type here because every
/// copy was identical. Outside the chunk a read falls through to the phase-wide neighbour reader
/// rather than answering air, which is what vanilla's own level lookup sees: reading air there
/// refused every patch that needs a block beneath it anywhere but its own chunk.
class PatchRegion {
public:
    PatchRegion(Chunk& chunk, ExternalStateReader externalBlockState, BlockWriteObserver observer)
        : chunk_(&chunk), externalBlockState_(std::move(externalBlockState)), observer_(std::move(observer)) {}

    [[nodiscard]] std::int32_t get(const DecorationPos& position) const;

    void set(const DecorationPos& position, std::int32_t stateId);

private:
    Chunk* chunk_;
    ExternalStateReader externalBlockState_;
    BlockWriteObserver observer_;
    std::vector<std::pair<DecorationPos, std::int32_t>> externalWrites_;
};

} // namespace basalt::server::world::terrain::decoration
