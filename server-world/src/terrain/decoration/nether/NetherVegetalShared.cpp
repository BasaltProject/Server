#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <cstdint>
#include <stdexcept>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

[[nodiscard]] bool isLayerEmpty(std::int32_t stateId) {
    const std::string path = blockPath(stateId);
    return registry::blocks::isAir(stateId) || path == "water" || path == "lava";
}

/// parity: the downward walk that numbers a column's ground layers from the surface.
[[nodiscard]] std::optional<std::int32_t> findGroundLayer(const Chunk& chunk, std::int32_t x, std::int32_t startY,
                                                          std::int32_t z, std::int32_t targetLayer) {
    std::int32_t currentLayer = 0;
    std::optional<std::int32_t> currentState = stateAt(chunk, x, startY, z);
    for (std::int32_t y = startY; y >= chunk.verticalBounds().minY() + 1; --y) {
        const std::optional<std::int32_t> below = stateAt(chunk, x, y - 1, z);
        const bool surface =
            below.has_value() && !isLayerEmpty(*below) && currentState.has_value() && isLayerEmpty(*currentState);
        if (surface && blockPath(*below) != "bedrock") {
            if (currentLayer == targetLayer) {
                return y;
            }
            currentLayer++;
        }
        currentState = below;
    }
    return std::nullopt;
}

} // namespace

std::int32_t stateId(const std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

std::optional<std::int32_t> terrainStateAt(const Chunk& chunk, const std::int32_t x, const std::int32_t y,
                                           const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return std::nullopt;
    }
    // parity: a spring counts surrounding blocks through the level, so it sees the next chunk.
    return chunk.decorationBlockStateId(x, y, z);
}

std::optional<std::int32_t> stateAt(const Chunk& chunk, const std::int32_t x, const std::int32_t y,
                                    const std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return std::nullopt;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

std::string blockPath(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

std::string blockPathAt(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() ? blockPath(*state) : std::string{};
}

bool isAir(const Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() && registry::blocks::isAir(*state);
}

void setBlock(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z,
              const std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

std::int32_t triangle(random::RandomSource& random, const std::int32_t range) {
    return random.nextInt(range + 1) - random.nextInt(range + 1);
}

std::vector<Pos> positionsOnEveryLayer(const Chunk& chunk, random::RandomSource& random, const std::int32_t count) {
    std::vector<Pos> positions;
    std::int32_t layer = 0;
    bool foundAny = false;
    do {
        foundAny = false;
        for (std::int32_t attempt = 0; attempt < count; ++attempt) {
            const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
            const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
            const std::int32_t startY = chunk.heightAt(x & 15, z & 15, HeightmapType::MotionBlocking);
            const std::optional<std::int32_t> y = findGroundLayer(chunk, x, startY, z, layer);
            if (y.has_value()) {
                positions.push_back(Pos{.x = x, .y = *y, .z = z});
                foundAny = true;
            }
        }
        layer++;
    } while (foundAny);
    return positions;
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
