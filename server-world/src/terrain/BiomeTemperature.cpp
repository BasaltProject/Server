#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/PerlinSimplexNoise.hpp>
#include <basalt/server/world/noise/SimplexNoise.hpp>
#include <basalt/server/world/terrain/BiomeClimateData.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>

#include <array>

namespace basalt::server::world::terrain::biomeTemperature {
namespace {

// Vanilla's own Overworld `sea_level`, hardcoded the same way in the surface rules.
constexpr std::int32_t kSeaLevel = 63;

/// The three fixed-seed noises are `Biome`'s own static fields, built straight off a legacy source:
/// the worldgen wrapper degenerates to a passthrough over a bit-based inner source anyway.
struct Noises {
    noise::PerlinSimplexNoise temperature;
    noise::PerlinSimplexNoise frozenTemperature;
    noise::SimplexNoise biomeInfo;
};

const Noises& noises() {
    static const Noises built = [] {
        random::RandomSource temperatureSource = random::RandomSource::legacy(1234LL);
        random::RandomSource frozenSource = random::RandomSource::legacy(3456LL);
        random::RandomSource infoSource = random::RandomSource::legacy(2345LL);
        const std::array<std::int32_t, 1> temperatureOctaves = {0};
        const std::array<std::int32_t, 3> frozenOctaves = {-2, -1, 0};
        return Noises{noise::PerlinSimplexNoise{temperatureSource, temperatureOctaves},
                      noise::PerlinSimplexNoise{frozenSource, frozenOctaves}, noise::SimplexNoise{infoSource}};
    }();
    return built;
}

// parity: Biome.TemperatureModifier.FROZEN.modifyTemperature.
[[nodiscard]] float frozenModifiedTemperature(std::int32_t x, std::int32_t z, float baseTemperature) {
    const double groundValueLargeVariation = noises().frozenTemperature.getValue(x * 0.05, z * 0.05) * 7.0;
    const double groundValueEdgeVariation = noises().biomeInfo.getValue(x * 0.2, z * 0.2);
    if (groundValueLargeVariation + groundValueEdgeVariation < 0.3) {
        if (noises().biomeInfo.getValue(x * 0.09, z * 0.09) < 0.8) {
            return 0.2F;
        }
    }
    return baseTemperature;
}

[[nodiscard]] float temperatureAt(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z) {
    const BiomeClimate climate = biomeClimateFor(biomeId);
    const float adjusted = climate.frozen ? frozenModifiedTemperature(x, z, climate.temperature) : climate.temperature;
    const std::int32_t snowLevel = kSeaLevel + 17;
    if (y <= snowLevel) {
        return adjusted;
    }
    const double v = noises().temperature.getValue(x / 8.0, z / 8.0) * 8.0;
    return adjusted - static_cast<float>((v + y - snowLevel) * 0.05 / 40.0);
}

} // namespace

bool coldEnoughToSnow(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z) {
    return temperatureAt(biomeId, x, y, z) < 0.15F;
}

bool shouldMeltFrozenOceanIcebergSlightly(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z) {
    return temperatureAt(biomeId, x, y, z) > 0.1F;
}

} // namespace basalt::server::world::terrain::biomeTemperature
