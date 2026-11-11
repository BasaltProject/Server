#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::plugins {

struct PluginTerrainShape {
    std::int32_t minY = 0;
    std::int32_t height = 0;
    std::int32_t seaLevel = 0;
    std::int32_t defaultBlock = 0;
    std::int32_t defaultFluid = 0;
    bool legacyRandomSource = false;
    bool disableMobGeneration = false;
};

struct PluginNoiseParameters {
    double baseAmplitude = 1.0;
    std::int32_t baseOctave = 0;
    std::int32_t octaveCount = 1;
    /// 0 disabled, 1 enabled, 2 legacy.
    std::int32_t normalize = 1;
    std::vector<double> amplitudeModifiers;
};

/// Where a noise's random source stood: a legacy one is its 48-bit state, a xoroshiro one its two words.
struct PluginNoiseSeed {
    bool legacyRandom = false;
    std::int64_t low = 0;
    std::int64_t high = 0;
    bool legacyNether = false;
};

/// Terrain noise of the game data, sampled for a seed without generating a chunk.
class PluginWorldgen {
public:
    PluginWorldgen() = default;
    PluginWorldgen(const PluginWorldgen&) = delete;
    PluginWorldgen& operator=(const PluginWorldgen&) = delete;
    PluginWorldgen(PluginWorldgen&&) = delete;
    PluginWorldgen& operator=(PluginWorldgen&&) = delete;
    virtual ~PluginWorldgen() = default;

    [[nodiscard]] static PluginWorldgen& unavailable();

    /// The `noise_settings` shape: `minecraft:overworld`, `minecraft:nether`, `minecraft:end` and the like.
    [[nodiscard]] virtual std::optional<PluginTerrainShape> shape(std::string_view settings) {
        static_cast<void>(settings);
        return std::nullopt;
    }

    /// A router function (`final_density`, `continents`, `erosion`, `depth`, `ridges`, `temperature`, `vegetation`).
    [[nodiscard]] virtual std::optional<double> density(std::string_view settings, std::int64_t seed,
                                                        std::string_view function, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(function);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    /// A density function given as game data JSON, evaluated with the random state of the settings and seed.
    [[nodiscard]] virtual std::optional<double> densityTree(std::string_view settings, std::int64_t seed,
                                                            std::string_view json, std::int32_t x, std::int32_t y,
                                                            std::int32_t z) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(json);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    /// A named noise (`minecraft:jagged`) seeded as the settings seed their random state.
    [[nodiscard]] virtual std::optional<double> noise(std::string_view settings, std::int64_t seed, std::string_view id,
                                                      double x, double y, double z) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(id);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    /// A noise built from the parameters of a game data noise (`minecraft:jagged`); a handle to sample, or nothing.
    [[nodiscard]] virtual std::optional<std::int64_t> createNoise(std::string_view id, const PluginNoiseSeed& seed) {
        static_cast<void>(id);
        static_cast<void>(seed);
        return std::nullopt;
    }

    /// A noise built from explicit parameters.
    [[nodiscard]] virtual std::optional<std::int64_t> createNoise(const PluginNoiseParameters& parameters,
                                                                  const PluginNoiseSeed& seed) {
        static_cast<void>(parameters);
        static_cast<void>(seed);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<double> sampleNoise(std::int64_t handle, double x, double y, double z) {
        static_cast<void>(handle);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<std::pair<double, double>> handleRange(std::int64_t handle) {
        static_cast<void>(handle);
        return std::nullopt;
    }

    virtual void releaseNoise(std::int64_t handle) { static_cast<void>(handle); }

    /// The smallest and largest value a named noise can return.
    [[nodiscard]] virtual std::optional<std::pair<double, double>> noiseRange(std::string_view settings,
                                                                              std::int64_t seed, std::string_view id) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(id);
        return std::nullopt;
    }

    /// One above the topmost block of the base terrain: non-air, or with `oceanFloor` also not a fluid.
    [[nodiscard]] virtual std::optional<std::int32_t> baseHeight(std::string_view settings, std::int64_t seed,
                                                                 std::int32_t x, std::int32_t z, bool oceanFloor) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(x);
        static_cast<void>(z);
        static_cast<void>(oceanFloor);
        return std::nullopt;
    }

    /// The block state ids of the base terrain column, bottom first.
    [[nodiscard]] virtual std::vector<std::int32_t> baseColumn(std::string_view settings, std::int64_t seed,
                                                               std::int32_t x, std::int32_t z) {
        static_cast<void>(settings);
        static_cast<void>(seed);
        static_cast<void>(x);
        static_cast<void>(z);
        return {};
    }
};

inline PluginWorldgen& PluginWorldgen::unavailable() {
    static PluginWorldgen none;
    return none;
}

} // namespace basalt::server::plugins
