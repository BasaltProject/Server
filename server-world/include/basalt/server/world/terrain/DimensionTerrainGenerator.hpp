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

/// parity: `noise_settings/nether.json` base shape.
[[nodiscard]] Chunk netherTerrain(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                                  std::int32_t chunkZ, std::int64_t seed = 0,
                                  NetherStructureGenerator* structureGenerator = nullptr);

void decorateNether(Chunk& chunk, const NetherBiomeSource& biomeSource, std::int64_t seed,
                    const NeighbourTerrain* neighbours = nullptr);

[[nodiscard]] Chunk nether(const NoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                           std::int32_t chunkZ, std::int64_t seed = 0,
                           NetherStructureGenerator* structureGenerator = nullptr);

/// parity: `noise_settings/end.json` base shape.
[[nodiscard]] Chunk endTerrain(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                               std::int32_t chunkZ, std::int64_t seed = 0,
                               EndCityStructureGenerator* structureGenerator = nullptr);

void decorateEnd(Chunk& chunk, const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int64_t seed);

[[nodiscard]] Chunk end(const NoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                        std::int32_t chunkZ, std::int64_t seed = 0,
                        EndCityStructureGenerator* structureGenerator = nullptr);

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
