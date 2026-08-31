#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

namespace basalt::server::world::terrain::decoration::tree {

bool isValidTreePos(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    return treeBlockRules::isValidTreeState(chunk.decorationBlockStateId(x, y, z));
}

void setBlock(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z,
              const std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    chunk.setDecorationBlockStateId(x, y, z, stateId);
}

} // namespace basalt::server::world::terrain::decoration::tree
