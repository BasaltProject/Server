#include <basalt/server/runtime/world/RealSurfaceEnvironment.hpp>
#include <basalt/server/world/WorldHeight.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

#include <cmath>

namespace basalt::server::runtime::world {
namespace {

namespace terrain = server::world::terrain;
namespace densityfunction = server::world::densityfunction;

/// parity: `SurfaceSystem.getSurfaceDepth`'s own three numbers.
constexpr double kDepthScale = 2.75;
constexpr double kDepthOffset = 3.0;
constexpr double kDepthJitter = 0.25;

[[nodiscard]] server::random::RandomSource clayBandSource(server::random::PositionalRandomFactory& random) {
    return random.fromHashOf("minecraft:clay_bands");
}

} // namespace

RealSurfaceEnvironment::RealSurfaceEnvironment(RealNoiseParameters::ReadText readText,
                                               RealNoiseParameters::ParseParameters parseParameters,
                                               server::random::PositionalRandomFactory random,
                                               const terrain::OverworldNoiseRouter& router, std::int64_t biomeZoomSeed)
    : biomeZoomSeed_(biomeZoomSeed), random_(random), router_(&router), biomeSource_(router, biomeZoomSeed),
      noises_(std::move(readText), std::move(parseParameters), random), clayBands_([&random] {
          server::random::RandomSource source = clayBandSource(random);
          return terrain::surface::BadlandsClayBands{source};
      }()) {}

double RealSurfaceEnvironment::noiseAt(std::string_view noiseId, double x, double y, double z) const {
    const server::world::noise::NormalNoise* noise = noises_.get(noiseId);
    return noise == nullptr ? 0.0 : noise->getValue(x, y, z);
}

std::int32_t RealSurfaceEnvironment::surfaceDepthNoise(std::int32_t x, std::int32_t z) const {
    const double value = noiseAt("surface", x, 0.0, z);
    const double jitter = random_.at(x, 0, z).nextDouble();
    return static_cast<std::int32_t>((value * kDepthScale) + kDepthOffset + (jitter * kDepthJitter));
}

double RealSurfaceEnvironment::surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const {
    return noiseAt("surface_secondary", x, 0.0, z);
}

std::int32_t RealSurfaceEnvironment::preliminarySurfaceLevel(std::int32_t x, std::int32_t z) const {
    // parity: the sample is quantized to its quart before the router is asked.
    densityfunction::FunctionContext context{.blockX = (x >> 2) << 2, .blockY = 0, .blockZ = (z >> 2) << 2};
    return static_cast<std::int32_t>(std::floor(router_->preliminarySurfaceLevel->compute(context)));
}

double RealSurfaceEnvironment::sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const {
    return noiseAt(noiseId, x, 0.0, z);
}

double RealSurfaceEnvironment::sample2dScaled(std::string_view noiseId, double x, double z) const {
    return noiseAt(noiseId, x, 0.0, z);
}

double RealSurfaceEnvironment::sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                        std::int32_t z) const {
    return noiseAt(noiseId, x, y, z);
}

float RealSurfaceEnvironment::verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
                                                     std::int32_t z) const {
    const std::scoped_lock lock{mutex_};
    const auto known = gradientRandoms_.find(name);
    if (known != gradientRandoms_.end()) {
        return known->second.at(x, y, z).nextFloat();
    }
    server::random::RandomSource source = random_.fromHashOf("minecraft:" + std::string{name});
    const auto added = gradientRandoms_.emplace(std::string{name}, source.forkPositional()).first;
    return added->second.at(x, y, z).nextFloat();
}

server::random::RandomSource RealSurfaceEnvironment::positionalRandom(std::int32_t x, std::int32_t y,
                                                                      std::int32_t z) const {
    return random_.at(x, y, z);
}

std::int32_t RealSurfaceEnvironment::estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const {
    // parity: a deliberate substitute for `NoiseChunk.preliminarySurfaceLevel`, scanning the
    // router's own `finalDensity` top down for the first solid block. Strictly more accurate for the
    // same purpose, and comparatively expensive: the terrain generator wraps this environment with
    // the already-filled chunk's heightmap, so this only runs when something asks it unwrapped.
    densityfunction::FunctionContext context{.blockX = x, .blockY = server::world::worldHeight::kMaxY, .blockZ = z};
    std::int32_t y = server::world::worldHeight::kMaxY;
    while (y >= server::world::worldHeight::kMinY && router_->finalDensity->compute(context) <= 0.0) {
        --y;
        context.blockY = y;
    }
    return y + 1;
}

const std::string& RealSurfaceEnvironment::nameOf(std::string_view biome) const {
    const auto known = biomeNames_.find(biome);
    if (known != biomeNames_.end()) {
        return known->second;
    }
    return biomeNames_.emplace(std::string{biome}, std::string{biome}).first->second;
}

const std::string& RealSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return biomeAt(x, y, z, nullptr);
}

const std::string& RealSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                   densityfunction::DensityCache* cache) const {
    const server::world::climate::BiomeName biome = biomeSource_.blockBiomeAt(x, y, z, cache);
    const std::scoped_lock lock{mutex_};
    return nameOf(biome);
}

const std::string& RealSurfaceEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                        densityfunction::DensityCache* cache) const {
    const server::world::climate::BiomeName biome = biomeSource_.biomeAtQuart(quartX, quartY, quartZ, cache);
    const std::scoped_lock lock{mutex_};
    return nameOf(biome);
}

std::int32_t RealSurfaceEnvironment::clayBand(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return clayBands_.stateId(y, noiseAt("clay_bands_offset", x, 0.0, z));
}

} // namespace basalt::server::runtime::world
