#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/plugins/ServerWorldgen.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>
#include <basalt/server/world/terrain/NoiseColumnHeights.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <cmath>

namespace basalt::server::runtime::plugins {
namespace {

namespace terrain = server::world::terrain;

[[nodiscard]] const terrain::Function* routerFunction(const terrain::NoiseRouter& router, std::string_view name) {
    if (name == "final_density") {
        return &router.finalDensity;
    }
    if (name == "continents") {
        return &router.continents;
    }
    if (name == "erosion") {
        return &router.erosion;
    }
    if (name == "depth") {
        return &router.depth;
    }
    if (name == "ridges") {
        return &router.ridges;
    }
    if (name == "temperature") {
        return &router.temperature;
    }
    if (name == "vegetation") {
        return &router.vegetation;
    }
    if (name == "preliminary_surface_level") {
        return &router.chunkSurfaceLevel;
    }
    return nullptr;
}

} // namespace

std::shared_ptr<const terrain::NoiseRouter> ServerWorldgen::router(std::string_view settings, std::int64_t seed) const {
    return hooks_.router ? hooks_.router(settings, seed) : nullptr;
}

std::optional<server::plugins::PluginTerrainShape> ServerWorldgen::shape(std::string_view settings) {
    const auto found = router(settings, 0);
    if (found == nullptr) {
        return std::nullopt;
    }
    return server::plugins::PluginTerrainShape{.minY = found->minY,
                                               .height = found->height,
                                               .seaLevel = found->seaLevel,
                                               .defaultBlock = found->defaultBlock,
                                               .defaultFluid = found->defaultFluid,
                                               .legacyRandomSource = found->legacyRandomSource,
                                               .disableMobGeneration = found->disableMobGeneration};
}

std::optional<double> ServerWorldgen::density(std::string_view settings, std::int64_t seed, std::string_view function,
                                              std::int32_t x, std::int32_t y, std::int32_t z) {
    const auto found = router(settings, seed);
    const terrain::Function* chosen = found == nullptr ? nullptr : routerFunction(*found, function);
    if (chosen == nullptr) {
        return std::nullopt;
    }
    const server::world::densityfunction::Sampler sampler = found->state->sampler(*chosen);
    return static_cast<double>(server::world::densityfunction::sampleUncached(*sampler, x, y, z));
}

std::optional<double> ServerWorldgen::densityTree(std::string_view settings, std::int64_t seed, std::string_view json,
                                                  std::int32_t x, std::int32_t y, std::int32_t z) {
    const auto found = router(settings, seed);
    const auto root = wire::common::parseJson(json);
    if (found == nullptr || !root.has_value()) {
        return std::nullopt;
    }
    try {
        const auto parsed = found->state->loader().parse(*root);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        const server::world::densityfunction::Sampler sampler = found->state->sampler(*parsed);
        return static_cast<double>(server::world::densityfunction::sampleUncached(*sampler, x, y, z));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<double> ServerWorldgen::noise(std::string_view settings, std::int64_t seed, std::string_view id, double x,
                                            double y, double z) {
    const auto found = router(settings, seed);
    if (found == nullptr) {
        return std::nullopt;
    }
    try {
        return static_cast<double>(found->state->noise(id)->get(x, y, z));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::int64_t ServerWorldgen::keep(std::shared_ptr<const server::world::noise::Noise> noise) {
    const std::int64_t handle = nextHandle_.fetch_add(1);
    const std::scoped_lock lock{noisesMutex_};
    noises_.emplace(handle, std::move(noise));
    return handle;
}

std::shared_ptr<const server::world::noise::Noise> ServerWorldgen::find(std::int64_t handle) const {
    const std::scoped_lock lock{noisesMutex_};
    const auto found = noises_.find(handle);
    return found == noises_.end() ? nullptr : found->second;
}

namespace {

[[nodiscard]] server::random::RandomSource randomOf(const server::plugins::PluginNoiseSeed& seed) {
    constexpr std::int64_t kMultiplier = 0x5DEECE66DLL;
    return seed.legacyRandom ? server::random::RandomSource::legacy(seed.low ^ kMultiplier)
                             : server::random::RandomSource::xoroshiro(seed.low, seed.high);
}

} // namespace

std::optional<std::int64_t> ServerWorldgen::createNoise(std::string_view id,
                                                        const server::plugins::PluginNoiseSeed& seed) {
    const auto found = router("minecraft:overworld", 0);
    if (found == nullptr) {
        return std::nullopt;
    }
    try {
        const auto parameters = found->state->loader().noise(id);
        if (!parameters.has_value()) {
            return std::nullopt;
        }
        server::random::RandomSource random = randomOf(seed);
        return keep(seed.legacyNether ? (*parameters)->createForLegacyNetherBiome(random)
                                      : (*parameters)->create(random));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::int64_t> ServerWorldgen::createNoise(const server::plugins::PluginNoiseParameters& parameters,
                                                        const server::plugins::PluginNoiseSeed& seed) {
    using Normalization = server::world::noise::NormalNoise::Normalization;
    server::world::noise::NormalNoise::Parameters built;
    built.baseAmplitude = parameters.baseAmplitude;
    built.baseOctave = parameters.baseOctave;
    built.octaveCount = parameters.octaveCount;
    built.normalize = parameters.normalize == 0   ? Normalization::Disabled
                      : parameters.normalize == 2 ? Normalization::Legacy
                                                  : Normalization::Enabled;
    built.amplitudeModifiers = parameters.amplitudeModifiers;
    try {
        const server::world::noise::NormalNoise noise{std::move(built)};
        server::random::RandomSource random = randomOf(seed);
        return keep(seed.legacyNether ? noise.createForLegacyNetherBiome(random) : noise.create(random));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<double> ServerWorldgen::sampleNoise(std::int64_t handle, double x, double y, double z) {
    const auto noise = find(handle);
    if (noise == nullptr) {
        return std::nullopt;
    }
    return static_cast<double>(std::isnan(z) ? noise->get(x, y) : noise->get(x, y, z));
}

std::optional<std::pair<double, double>> ServerWorldgen::handleRange(std::int64_t handle) {
    const auto noise = find(handle);
    if (noise == nullptr) {
        return std::nullopt;
    }
    const server::world::noise::Interval range = noise->range();
    return std::make_pair(static_cast<double>(range.min()), static_cast<double>(range.max()));
}

void ServerWorldgen::releaseNoise(std::int64_t handle) {
    const std::scoped_lock lock{noisesMutex_};
    noises_.erase(handle);
}

std::optional<std::pair<double, double>> ServerWorldgen::noiseRange(std::string_view settings, std::int64_t seed,
                                                                    std::string_view id) {
    const auto found = router(settings, seed);
    if (found == nullptr) {
        return std::nullopt;
    }
    try {
        const server::world::noise::Interval range = found->state->noise(id)->range();
        return std::make_pair(static_cast<double>(range.min()), static_cast<double>(range.max()));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::int32_t> ServerWorldgen::baseHeight(std::string_view settings, std::int64_t seed, std::int32_t x,
                                                       std::int32_t z, bool oceanFloor) {
    const auto found = router(settings, seed);
    if (found == nullptr) {
        return std::nullopt;
    }
    const terrain::NoiseColumnHeights heights{*found};
    return oceanFloor ? heights.oceanFloorHeight(x, z) : heights.surfaceHeight(x, z);
}

std::vector<std::int32_t> ServerWorldgen::baseColumn(std::string_view settings, std::int64_t seed, std::int32_t x,
                                                     std::int32_t z) {
    const auto found = router(settings, seed);
    return found == nullptr ? std::vector<std::int32_t>{} : terrain::NoiseColumnHeights{*found}.column(x, z);
}

} // namespace basalt::server::runtime::plugins
