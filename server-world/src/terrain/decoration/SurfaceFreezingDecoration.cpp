#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/BiomeClimateData.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>
#include <basalt/server/world/terrain/decoration/SurfaceFreezingDecoration.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

const std::set<std::string, std::less<>>& cannotSupportSnowLayer() {
    static const std::set<std::string, std::less<>> paths = {"ice", "packed_ice", "barrier"};
    return paths;
}

const std::set<std::string, std::less<>>& supportOverrideSnowLayer() {
    static const std::set<std::string, std::less<>> paths = {"honey_block", "soul_sand", "mud"};
    return paths;
}

[[nodiscard]] bool shouldFreeze(const Chunk& chunk, std::int32_t localX, std::int32_t y, std::int32_t localZ,
                                std::string_view biomeId, std::int32_t worldX, std::int32_t worldZ) {
    return biomeTemperature::coldEnoughToSnow(biomeId, worldX, y, worldZ) &&
           pathOf(chunk.blockStateId(localX, y, localZ)) == "water";
}

[[nodiscard]] bool shouldSnow(const Chunk& chunk, std::int32_t localX, std::int32_t y, std::int32_t localZ,
                              std::string_view biomeId, std::int32_t worldX, std::int32_t worldZ, ChunkHeight bounds) {
    if (y <= bounds.minY()) {
        return false;
    }
    const BiomeClimate climate = biomeClimateFor(biomeId);
    if (!climate.hasPrecipitation || !biomeTemperature::coldEnoughToSnow(biomeId, worldX, y, worldZ)) {
        return false;
    }
    const std::int32_t stateId = chunk.blockStateId(localX, y, localZ);
    if (!registry::blocks::isAir(stateId) && pathOf(stateId) != "snow") {
        return false;
    }
    const std::int32_t belowStateId = chunk.blockStateId(localX, y - 1, localZ);
    const std::string belowPath = pathOf(belowStateId);
    if (cannotSupportSnowLayer().find(belowPath) != cannotSupportSnowLayer().end()) {
        return false;
    }
    return supportOverrideSnowLayer().find(belowPath) != supportOverrideSnowLayer().end() ||
           registry::blockShapes::isCollisionFaceFull(belowStateId, registry::BlockFace::Up);
}

void freezeColumn(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment, std::int32_t localX,
                  std::int32_t localZ) {
    const std::int32_t worldX = chunk.chunkX() * 16 + localX;
    const std::int32_t worldZ = chunk.chunkZ() * 16 + localZ;
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t topY = chunk.heightAt(localX, localZ, HeightmapType::MotionBlocking);
    if (topY < bounds.minY() || topY > bounds.maxY()) {
        return;
    }

    const std::string& biomeId = environment.biomeAt(worldX, topY, worldZ);
    const std::int32_t belowY = topY - 1;
    if (belowY >= bounds.minY() && shouldFreeze(chunk, localX, belowY, localZ, biomeId, worldX, worldZ)) {
        chunk.setBlockStateId(localX, belowY, localZ, stateIdOf("ice"));
    }

    if (shouldSnow(chunk, localX, topY, localZ, biomeId, worldX, worldZ, bounds)) {
        chunk.setBlockStateId(localX, topY, localZ, stateIdOf("snow"));
        const std::optional<std::int32_t> snowyBelow =
            registry::blockStateFamilies::withSnowy(chunk.blockStateId(localX, belowY, localZ), true);
        if (snowyBelow.has_value()) {
            chunk.setBlockStateId(localX, belowY, localZ, *snowyBelow);
        }
    }
}

} // namespace

void surfaceFreezingDecoration::placeAt(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment,
                                        std::int32_t x, std::int32_t z) {
    for (std::int32_t dx = 0; dx < 16; ++dx) {
        for (std::int32_t dz = 0; dz < 16; ++dz) {
            const std::int32_t worldX = x + dx;
            const std::int32_t worldZ = z + dz;
            if ((worldX >> 4) != chunk.chunkX() || (worldZ >> 4) != chunk.chunkZ()) {
                continue;
            }
            freezeColumn(chunk, environment, worldX & 15, worldZ & 15);
        }
    }
}

void surfaceFreezingDecoration::decorate(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment) {
    placeAt(chunk, environment, chunk.chunkX() * 16, chunk.chunkZ() * 16);
}

} // namespace basalt::server::world::terrain::decoration
