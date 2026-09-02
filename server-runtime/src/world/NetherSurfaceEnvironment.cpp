#include <basalt/server/runtime/world/NetherSurfaceEnvironment.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>

namespace basalt::server::runtime::world {
namespace {

namespace terrain = server::world::terrain;
namespace densityfunction = server::world::densityfunction;

/// parity: `SurfaceSystem.getSurfaceDepth`'s own three numbers.
constexpr double kDepthScale = 2.75;
constexpr double kDepthOffset = 3.0;
constexpr double kDepthJitter = 0.25;

/// parity: the Nether is 128 blocks tall and starts at zero, so its roof is the scan's own start.
constexpr std::int32_t kRoof = 127;
constexpr std::int32_t kFloor = 0;

} // namespace

NetherSurfaceEnvironment::NetherSurfaceEnvironment(RealNoiseParameters::ReadText readText,
                                                   RealNoiseParameters::ParseParameters parseParameters,
                                                   server::random::PositionalRandomFactory random,
                                                   const terrain::NetherNoiseRouter& router,
                                                   const terrain::NetherBiomeSource& biomeSource,
                                                   std::int64_t biomeZoomSeed)
    : biomeZoomSeed_(biomeZoomSeed), random_(random), router_(&router), biomeSource_(&biomeSource),
      noises_(std::move(readText), std::move(parseParameters), random) {}

double NetherSurfaceEnvironment::noiseAt(std::string_view noiseId, double x, double y, double z) const {
    const server::world::noise::NormalNoise* noise = noises_.get(noiseId);
    return noise == nullptr ? 0.0 : noise->getValue(x, y, z);
}

std::int32_t NetherSurfaceEnvironment::surfaceDepthNoise(std::int32_t x, std::int32_t z) const {
    const double value = noiseAt("surface", x, 0.0, z);
    const double jitter = random_.at(x, 0, z).nextDouble();
    return static_cast<std::int32_t>((value * kDepthScale) + kDepthOffset + (jitter * kDepthJitter));
}

double NetherSurfaceEnvironment::surfaceSecondaryNoise(std::int32_t x, std::int32_t z) const {
    return noiseAt("surface_secondary", x, 0.0, z);
}

double NetherSurfaceEnvironment::sample2d(std::string_view noiseId, std::int32_t x, std::int32_t z) const {
    return noiseAt(noiseId, x, 0.0, z);
}

double NetherSurfaceEnvironment::sample3d(std::string_view noiseId, std::int32_t x, std::int32_t y,
                                          std::int32_t z) const {
    return noiseAt(noiseId, x, y, z);
}

float NetherSurfaceEnvironment::verticalGradientRandom(std::string_view name, std::int32_t x, std::int32_t y,
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

server::random::RandomSource NetherSurfaceEnvironment::positionalRandom(std::int32_t x, std::int32_t y,
                                                                        std::int32_t z) const {
    return random_.at(x, y, z);
}

std::int32_t NetherSurfaceEnvironment::estimatedSurfaceHeight(std::int32_t x, std::int32_t z) const {
    densityfunction::FunctionContext context{.blockX = x, .blockY = kRoof, .blockZ = z};
    std::int32_t y = kRoof;
    while (y >= kFloor && router_->finalDensity->compute(context) <= 0.0) {
        --y;
        context.blockY = y;
    }
    return y + 1;
}

const std::string& NetherSurfaceEnvironment::nameOf(std::string_view biome) const {
    const auto known = biomeNames_.find(biome);
    if (known != biomeNames_.end()) {
        return known->second;
    }
    return biomeNames_.emplace(std::string{biome}, std::string{biome}).first->second;
}

const std::string& NetherSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z) const {
    return biomeAt(x, y, z, nullptr);
}

const std::string& NetherSurfaceEnvironment::biomeAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                                     densityfunction::DensityCache* cache) const {
    const std::string_view biome = biomeSource_->blockBiomeAt(x, y, z, cache);
    const std::scoped_lock lock{mutex_};
    return nameOf(biome);
}

const std::string& NetherSurfaceEnvironment::noiseBiomeAt(std::int32_t quartX, std::int32_t quartY, std::int32_t quartZ,
                                                          densityfunction::DensityCache* cache) const {
    const std::string_view biome = biomeSource_->biomeAtQuart(quartX, quartY, quartZ, cache);
    const std::scoped_lock lock{mutex_};
    return nameOf(biome);
}

} // namespace basalt::server::runtime::world
