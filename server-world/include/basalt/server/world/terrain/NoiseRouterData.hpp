#pragma once

#include <basalt/server/world/climate/Climate.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/RandomState.hpp>

#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::material {
class MaterialRule;
class MaterialSystem;
} // namespace basalt::server::world::terrain::material

namespace basalt::server::world::terrain::carver {
class CarverRegistry;
}

namespace basalt::server::world::terrain {

using densityfunction::Function;

/// parity: `Aquifer.Config`, the six functions a noise-based aquifer reads.
struct AquiferFunctions {
    Function barrier;
    Function fluidLevelFloodedness;
    Function fluidLevelSpread;
    Function lava;
    Function exclusion;
    Function surfaceLevel;
};

/// parity: `SpawnTargetPoint`, a climate niche over named density functions.
struct SpawnTarget {
    std::vector<std::pair<Function, climate::Parameter>> parameters;
};

/// parity: `NoiseGeneratorSettings` with its `NoiseRouter`.
struct NoiseRouter {
    std::shared_ptr<densityfunction::RandomState> state;
    Function temperature;
    Function vegetation;
    Function continents;
    Function erosion;
    Function depth;
    Function ridges;
    Function chunkSurfaceLevel;
    Function finalDensity;
    std::optional<AquiferFunctions> aquifers;
    std::shared_ptr<const material::MaterialRule> materialRule;
    std::shared_ptr<material::MaterialSystem> materialSystem;
    std::shared_ptr<carver::CarverRegistry> carvers;
    std::vector<SpawnTarget> spawnTarget;
    std::int32_t minY = -64;
    std::int32_t height = 384;
    std::int32_t seaLevel = 63;
    std::int32_t defaultBlock = 0;
    std::int32_t defaultFluid = 0;
    bool legacyRandomSource = false;
    bool disableMobGeneration = false;
};

namespace noiseRouterData {

[[nodiscard]] std::expected<NoiseRouter, std::string> load(std::string_view settingsId, std::int64_t seed,
                                                           const densityfunction::WorldgenReader& reader);

} // namespace noiseRouterData

} // namespace basalt::server::world::terrain
