#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/GeneratedBlockWrites.hpp>
#include <basalt/server/world/terrain/decoration/OreBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/VegetationBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>

#include <cstdint>

namespace basalt::server::world::generatedBlockWrites {
namespace {

[[nodiscard]] bool insideChunk(const Chunk& chunk, std::int32_t y) {
    return y >= chunk.verticalBounds().minY() && y <= chunk.verticalBounds().maxY();
}

} // namespace

std::int64_t cellKey(const ChunkGeneratedBlockWrite& write) {
    return (static_cast<std::int64_t>(write.x & 15) << 40) | (static_cast<std::int64_t>(write.z & 15) << 32) |
           (static_cast<std::int64_t>(write.y) & 0xFFFFFFFFLL);
}

bool apply(Chunk& chunk, const ChunkGeneratedBlockWrite& write, const bool sourceGoesOnTop,
           const BatchWritten* queued) {
    if (!insideChunk(chunk, write.y)) {
        return false;
    }
    const std::int32_t localX = write.x & 15;
    const std::int32_t localZ = write.z & 15;
    const std::int32_t current = chunk.blockStateId(localX, write.y, localZ);
    // Earlier writes of this same batch are not the receiver's decoration.
    const bool untouched = current == write.expectedStateId || (queued != nullptr && queued->contains(cellKey(write)));
    // A chunk that decorates later covers this write only with a feature that could have replaced it.
    const bool onTop = sourceGoesOnTop || untouched ||
                       terrain::decoration::oreBlockRules::survivesLaterDecoration(write.policy, write.stateId);
    const bool loosely =
        onTop && terrain::decoration::oreBlockRules::canReplaceDeferred(write.policy, current, write.y);
    const bool canReplace = write.replaceAnyState || current == write.expectedStateId ||
                            (onTop && write.policy == GeneratedBlockWritePolicy::Tree &&
                             terrain::decoration::tree::treeBlockRules::canReplaceDeferred(current, write.stateId)) ||
                            loosely;
    if (!canReplace) {
        return false;
    }
    const std::int32_t stateId =
        write.policy == GeneratedBlockWritePolicy::Tree
            ? terrain::decoration::tree::treeBlockRules::resolveDeferredState(current, write.stateId)
            : write.stateId;
    const std::int32_t below = write.y > chunk.verticalBounds().minY() ? chunk.blockStateId(localX, write.y - 1, localZ)
                                                                       : registry::blocks::air().stateId;
    if (!terrain::decoration::vegetationBlockRules::survivesDeferred(stateId, below)) {
        return false;
    }
    chunk.setBlockStateId(localX, write.y, localZ, stateId);
    return true;
}

} // namespace basalt::server::world::generatedBlockWrites
