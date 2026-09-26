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

/// parity: `noise_settings/nether.json` base shape, block and fluid defaults and cell sizes, with its
/// structures: everything a chunk holds before it decorates.
[[nodiscard]] Chunk netherTerrain(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                                  std::int32_t chunkZ, std::int64_t seed = 0,
                                  NetherStructureGenerator* structureGenerator = nullptr);

/// The Nether's decoration steps. `neighbours` answers past the chunk edge; without it the outside
/// reads as nothing and what is written there is deferred.
void decorateNether(Chunk& chunk, const NetherBiomeSource& biomeSource, std::int64_t seed,
                    const NeighbourTerrain* neighbours = nullptr);

/// One Nether chunk decorated on its own.
[[nodiscard]] Chunk nether(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                           std::int32_t chunkZ, std::int64_t seed = 0,
                           NetherStructureGenerator* structureGenerator = nullptr);

/// parity: `noise_settings/end.json` base shape, block default and cell sizes, with the End cities.
[[nodiscard]] Chunk endTerrain(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                               std::int32_t chunkZ, std::int64_t seed = 0,
                               EndCityStructureGenerator* structureGenerator = nullptr);

void decorateEnd(Chunk& chunk, const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int64_t seed);

/// One End chunk decorated on its own.
[[nodiscard]] Chunk end(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                        std::int32_t chunkZ, std::int64_t seed = 0,
                        EndCityStructureGenerator* structureGenerator = nullptr);

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
