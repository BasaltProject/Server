#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

std::int32_t PatchRegion::get(const DecorationPos& position) const {
    const ChunkHeight bounds = chunk_->verticalBounds();
    if (position.y < bounds.minY() || position.y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
        return chunk_->blockStateId(position.x & 15, position.y, position.z & 15);
    }
    for (const auto& [written, stateId] : externalWrites_) {
        if (written == position) {
            return stateId;
        }
    }
    // parity: what this chunk already queued for the neighbour is in the neighbour's proto chunk as
    // far as vanilla is concerned, so it beats whatever the neighbour view still reports.
    if (const std::optional<std::int32_t> queued = chunk_->queuedDecorationStateId(position.x, position.y, position.z);
        queued.has_value()) {
        return *queued;
    }
    if (externalBlockState_) {
        if (const std::optional<std::int32_t> observed = externalBlockState_(position.x, position.y, position.z);
            observed.has_value()) {
            return *observed;
        }
    }
    return chunk_->decorationBlockStateId(position.x, position.y, position.z);
}

void PatchRegion::set(const DecorationPos& position, std::int32_t stateId) {
    if ((position.x >> 4) == chunk_->chunkX() && (position.z >> 4) == chunk_->chunkZ()) {
        chunk_->setBlockStateId(position.x & 15, position.y, position.z & 15, stateId);
    } else {
        setExternalDecorationBlockStateId(*chunk_, externalWrites_, externalBlockState_, position, stateId);
    }
    if (observer_) {
        observer_(position.x, position.y, position.z, stateId);
    }
}

} // namespace basalt::server::world::terrain::decoration
