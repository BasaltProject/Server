#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace basalt::server::world::terrain {

using densityfunction::Function;

/// Resolves a named noise parameter. The graph shape below is code, but the parameters it names
/// come from `data/minecraft/worldgen/noise/`, loaded at runtime and never embedded here.
using NoiseLookup = std::function<std::shared_ptr<const noise::NormalNoise>(std::string_view id)>;

/// parity: `net.minecraft.world.level.levelgen.NoiseRouterData`, the `overworld` assembly only.
///
/// Gap: only the non-large-biomes non-amplified variant.
struct OverworldNoiseRouter {
    Function barrierNoise;
    Function fluidLevelFloodednessNoise;
    Function fluidLevelSpreadNoise;
    Function lavaNoise;
    Function shiftX;
    Function shiftZ;
    Function temperature;
    Function vegetation;
    Function continents;
    Function erosion;
    Function depth;
    Function ridges;
    Function preliminarySurfaceLevel;
    Function finalDensity;
    Function veinToggle;
    Function veinRidged;
    Function veinGap;
};

/// The three non-zero functions in vanilla's Nether router.
struct NetherNoiseRouter {
    Function temperature;
    Function vegetation;
    Function finalDensity;
};

/// The two non-zero functions in vanilla's End router.
struct EndNoiseRouter {
    Function erosion;
    Function finalDensity;
};

namespace noiseRouterData {

[[nodiscard]] OverworldNoiseRouter overworld(const NoiseLookup& noise, Function base3d);

/// parity: `NoiseRouterData.nether`, using `noise_settings/nether.json`.
[[nodiscard]] NetherNoiseRouter nether(const NoiseLookup& noise, Function base3d);

/// parity: `NoiseRouterData.end`, using `noise_settings/end.json`.
[[nodiscard]] EndNoiseRouter end(Function base3d, std::int64_t seed);

} // namespace noiseRouterData

} // namespace basalt::server::world::terrain
