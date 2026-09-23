#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/noise/Noise.hpp>
#include <basalt/server/world/terrain/BiomeClimateData.hpp>
#include <basalt/server/world/terrain/BiomeTemperature.hpp>

#include <cstdint>
#include <memory>

namespace basalt::server::world::terrain::biomeTemperature {
namespace {

// Vanilla's own Overworld `sea_level`, hardcoded the same way in the surface rules.
constexpr std::int32_t kSeaLevel = 63;

/// parity: `Biome`'s three fixed-seed static noises, simplex since 26.3.
struct Noises {
    std::unique_ptr<noise::SimplexNoise> temperature;
    std::unique_ptr<noise::NoiseStack> frozenTemperature;
    std::unique_ptr<noise::SimplexNoise> biomeInfo;
};

const Noises& noises() {
    static const Noises built = [] {
        random::RandomSource temperatureSource = random::RandomSource::legacy(1234LL);
        random::RandomSource frozenSource = random::RandomSource::legacy(3456LL);
        random::RandomSource infoSource = random::RandomSource::legacy(2345LL);
        std::vector<noise::NoiseStack::Layer> frozen;
        frozen.push_back({std::make_shared<const noise::SimplexNoise>(frozenSource, true), 1.0, 0.14285715F});
        frozen.push_back({std::make_shared<const noise::SimplexNoise>(frozenSource, true), 0.5, 0.2857143F});
        frozen.push_back({std::make_shared<const noise::SimplexNoise>(frozenSource, true), 0.25, 0.5714286F});
        return Noises{std::make_unique<noise::SimplexNoise>(temperatureSource, true),
                      std::make_unique<noise::NoiseStack>(std::move(frozen)),
                      std::make_unique<noise::SimplexNoise>(infoSource, true)};
    }();
    return built;
}

// parity: Biome.TemperatureModifier.FROZEN.modifyTemperature.
[[nodiscard]] float frozenModifiedTemperature(std::int32_t x, std::int32_t z, float baseTemperature) {
    const auto large = static_cast<double>(noises().frozenTemperature->get(x * 0.05, z * 0.05) * 7.0F);
    const auto edge = static_cast<double>(noises().biomeInfo->get(x * 0.2, z * 0.2));
    if (large + edge < 0.3 && static_cast<double>(noises().biomeInfo->get(x * 0.09, z * 0.09)) < 0.8) {
        return 0.2F;
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
    const float v = noises().temperature->get(static_cast<double>(static_cast<float>(x) / 8.0F),
                                              static_cast<double>(static_cast<float>(z) / 8.0F)) *
                    8.0F;
    return adjusted - (v + static_cast<float>(y) - static_cast<float>(snowLevel)) * 0.05F / 40.0F;
}

} // namespace

bool coldEnoughToSnow(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z) {
    return temperatureAt(biomeId, x, y, z) < 0.15F;
}

bool shouldMeltFrozenOceanIcebergSlightly(std::string_view biomeId, std::int32_t x, std::int32_t y, std::int32_t z) {
    return temperatureAt(biomeId, x, y, z) > 0.1F;
}

} // namespace basalt::server::world::terrain::biomeTemperature
