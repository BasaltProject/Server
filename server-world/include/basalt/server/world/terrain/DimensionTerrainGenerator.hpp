#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/EndCityStructureGenerator.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>
#include <basalt/server/world/terrain/NetherStructureGenerator.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/RealTerrainGenerator.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>

namespace basalt::server::world::terrain::dimensionTerrainGenerator {

/// parity: `noise_settings/nether.json` base shape, block and fluid defaults and cell sizes.
/// `neighbours` lets a decoration body read past the chunk edge. Without it the outside reads as
/// nothing, a spring that counts five rocks around itself never finds them on a border, and every
/// cross-chunk write is refused on delivery because its guard was filled in with air.
[[nodiscard]] Chunk nether(const NetherNoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                           std::int32_t chunkZ, const surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr,
                           std::int64_t seed = 0, NetherStructureGenerator* structureGenerator = nullptr,
                           NeighbourTerrainCache* neighbours = nullptr,
                           const realTerrainGenerator::BeforeDecoration& beforeDecoration = {});

/// The same chunk stopped before decoration, which is what a neighbour read is allowed to see.
/// Generating a decorated neighbour would recurse.
[[nodiscard]] Chunk netherTerrain(const NetherNoiseRouter& router, const NetherBiomeSource& biomeSource,
                                  std::int32_t chunkX, std::int32_t chunkZ,
                                  const surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr,
                                  std::int64_t seed = 0, NetherStructureGenerator* structureGenerator = nullptr);

/// parity: `noise_settings/end.json` base shape, block default and cell sizes.
[[nodiscard]] Chunk end(const EndNoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                        std::int32_t chunkZ, std::int64_t seed = 0,
                        EndCityStructureGenerator* structureGenerator = nullptr,
                        const realTerrainGenerator::BeforeDecoration& beforeDecoration = {});

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
