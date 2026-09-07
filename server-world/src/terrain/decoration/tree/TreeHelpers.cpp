#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeBlockRules.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeHelpers.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

/// parity: `FluidState.isSourceOfType(WATER)`, unlike the root placer's any-water check.
[[nodiscard]] bool hasSourceWaterFluid(const std::int32_t stateId) {
    const std::int32_t water = registry::blocks::water().stateId;
    return registry::blockFluids::hasWaterFluid(stateId) && !(stateId > water && stateId <= water + 15);
}

[[nodiscard]] std::string pathOf(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

/// What the below-trunk provider leaves alone: soil a tree may already be standing on.
const std::set<std::string, std::less<>>& cannotReplaceBelowTrunkPaths() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt", "coarse_dirt", "rooted_dirt", "mud", "muddy_mangrove_roots", "moss_block", "pale_moss_block", "podzol"};
    return paths;
}

} // namespace

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

void placeBelowTrunkBlock(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    if (!cannotReplaceBelowTrunkPaths().contains(pathOf(chunk.decorationBlockStateId(x, y, z)))) {
        setBlock(chunk, x, y, z, registry::blocks::named("dirt")->stateId);
    }
}

std::int32_t decorationHeightAt(const Chunk& chunk, const std::int32_t worldX, const std::int32_t worldZ,
                                const HeightmapType type) {
    if ((worldX >> 4) == chunk.chunkX() && (worldZ >> 4) == chunk.chunkZ()) {
        return chunk.heightAt(worldX & 15, worldZ & 15, type);
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t y = bounds.maxY(); y >= bounds.minY(); --y) {
        if (heightmapMatches(type, chunk.decorationBlockStateId(worldX, y, worldZ))) {
            return y + 1;
        }
    }
    return bounds.minY();
}

bool canPlaceLeaf(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    const std::map<std::string, std::string> properties =
        registry::blockStateDefinitions::propertiesOf(chunk.decorationBlockStateId(x, y, z));
    const auto persistent = properties.find("persistent");
    if (persistent != properties.end() && persistent->second == "true") {
        return false;
    }
    return isValidTreePos(chunk, x, y, z);
}

bool tryPlaceLeaf(Chunk& chunk, const std::int32_t foliageStateId, const std::int32_t x, const std::int32_t y,
                  const std::int32_t z) {
    if (!canPlaceLeaf(chunk, x, y, z)) {
        return false;
    }
    std::int32_t placedStateId = foliageStateId;
    std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(foliageStateId);
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(foliageStateId);
    if (block != nullptr && properties.contains("waterlogged")) {
        properties["waterlogged"] = hasSourceWaterFluid(chunk.decorationBlockStateId(x, y, z)) ? "true" : "false";
        placedStateId = registry::blocks::stateIdOf(*block, properties).value_or(foliageStateId);
    }
    chunk.setDecorationBlockStateId(x, y, z, placedStateId);
    return true;
}

} // namespace basalt::server::world::terrain::decoration::tree
