#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/EndCityStructureGenerator.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>
#include <basalt/server/world/terrain/NetherStructureGenerator.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/TheEndBiomeSource.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::dimensionTerrainGenerator {

/// parity: `noise_settings/nether.json` base shape, block and fluid defaults and cell sizes.
[[nodiscard]] Chunk nether(const NetherNoiseRouter& router, const NetherBiomeSource& biomeSource, std::int32_t chunkX,
                           std::int32_t chunkZ, const surface::SurfaceRuleEnvironment* surfaceEnvironment = nullptr,
                           std::int64_t seed = 0, NetherStructureGenerator* structureGenerator = nullptr);

/// parity: `noise_settings/end.json` base shape, block default and cell sizes.
[[nodiscard]] Chunk end(const EndNoiseRouter& router, const TheEndBiomeSource& biomeSource, std::int32_t chunkX,
                        std::int32_t chunkZ, std::int64_t seed = 0,
                        EndCityStructureGenerator* structureGenerator = nullptr);

} // namespace basalt::server::world::terrain::dimensionTerrainGenerator
