#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/BubbleColumnFormation.hpp>

#include <optional>
#include <string>

namespace basalt::server::world::terrain {
namespace {

[[nodiscard]] std::int32_t columnState(bool drag) {
    return registry::blocks::stateIdOf(wire::common::Identifier::vanilla("bubble_column"),
                                       {{"drag", drag ? "true" : "false"}})
        .value_or(registry::blocks::air().stateId);
}

/// parity: `getColumnState`. Soul sand pushes up, magma drags down, anything else grows nothing.
[[nodiscard]] std::optional<std::int32_t> columnStateFor(std::int32_t belowStateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(belowStateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    if (block->path() == "soul_sand") {
        return columnState(false);
    }
    if (block->path() == "magma_block") {
        return columnState(true);
    }
    return std::nullopt;
}

} // namespace

void bubbleColumnFormation::formInto(Chunk& chunk) {
    const std::int32_t water = registry::blocks::water().stateId;
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t z = 0; z < 16; ++z) {
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t y = bounds.minY() + 1; y <= bounds.maxY(); ++y) {
                if (chunk.blockStateId(x, y, z) != water) {
                    continue;
                }
                const std::optional<std::int32_t> columnStateId = columnStateFor(chunk.blockStateId(x, y - 1, z));
                if (!columnStateId.has_value()) {
                    continue;
                }
                for (std::int32_t top = y; top <= bounds.maxY() && chunk.blockStateId(x, top, z) == water; ++top) {
                    chunk.setBlockStateId(x, top, z, *columnStateId);
                }
            }
        }
    }
}

} // namespace basalt::server::world::terrain
